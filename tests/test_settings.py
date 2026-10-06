"""Run the actual freestanding PowerPC settings codec/service and hook macros.

Build tests/settings-test.elf into build first (see DEVELOPMENT.md).
Requires Unicorn, installed into build/test-deps or the Python environment.
"""
from pathlib import Path
import random
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn import ppc_const as reg

RECORD = 0x8045A6C0 + 0x1F24
STOP = 0x100000
IDS = [0, 1, 3, 5, 8, 9, 10, 12, 13, 14, 16, 18, 19, 20, 21, 22, 24, 26, 28]


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x100000, 0x100000)
        self.cpu.mem_map(0x80000000, 0x500000)
        # Verified against the local native DOL, function 0x800C0658.
        self.cpu.mem_write(0x800C0658, bytes.fromhex("800304302c0000004182000c386304084e800020386304884e800020"))
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Enable FPR saves in native hooks.
        elf = (ROOT / "build/settings-test.elf").read_bytes()
        shoff = struct.unpack_from(">I", elf, 0x20)[0]
        shsize, shnum = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, shoff + i * shsize) for i in range(shnum)]
        self.symbols = {}
        for s in sections:
            _, kind, flags, address, offset, size, link, _, _, entsize = s
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[offset:offset + size])
            if kind == 2:
                strings = sections[link]
                table = elf[strings[4]:strings[4] + strings[5]]
                for i in range(0, size, entsize):
                    name, value, _, _, _, _ = struct.unpack_from(">IIIBBH", elf, offset + i)
                    text = table[name:].split(b"\0", 1)[0].decode()
                    self.symbols[text] = value
        self.cpu.reg_write(gpr(1), 0x80400000)
        self.cpu.reg_write(gpr(2), 0x80300000)
        # Function slots appended after the original 27 exports.
        for slot, name in [(27, "Settings_Get"), (28, "Settings_Set")]:
            self.cpu.mem_write(0x80300000 - 200 + slot * 4, struct.pack(">I", self.symbols[name]))
        self.cpu.mem_write(0x80000000, b"TYRE01")
        self.cpu.mem_write(RECORD - 8, b"\xA5" * 60)

    def call(self, name, *args):
        for i, value in enumerate(args, 3):
            self.cpu.reg_write(gpr(i), value & 0xFFFFFFFF)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.symbols[name], STOP, count=300000)
        if self.cpu.reg_read(reg.UC_PPC_REG_PC) != STOP:
            raise AssertionError(f"{name} did not return")
        return self.cpu.reg_read(gpr(3))

    def record(self):
        return bytes(self.cpu.mem_read(RECORD, 44))

    def put(self, data):
        assert len(data) == 44
        self.cpu.mem_write(RECORD, bytes(data))

    def init(self):
        self.call("TMSettings_Init", RECORD)

    def read(self, field, index=0):
        return self.call("TMSettings_Read", RECORD, field, index)

    def write(self, field, index, value):
        return self.call("TMSettings_Write", RECORD, field, index, value)


class RelocatedDATSettingsTests(unittest.TestCase):
    """Exercise hmex's emitted DAT with the native MEX relocation rules.

    The original ELF tests cannot catch relocation-table addend loss. Requires
    build/settings-relocated-test.dat from the runner; the input is never modified.
    """
    dat_path = ROOT / "build/settings-relocated-test.dat"

    def setUp(self):
        self.m = Machine()
        self.m.cpu.mem_map(0x80500000, 0x1300000)
        dat = self.dat_path.read_bytes()
        _, data_size, reloc_count, root_count, ref_count = struct.unpack_from(">5I", dat)
        nodes = 32 + data_size + reloc_count * 4
        strings = nodes + (root_count + ref_count) * 8
        header = None
        for i in range(root_count):
            offset, name = struct.unpack_from(">II", dat, nodes + i * 8)
            if dat[strings + name:].split(b"\0", 1)[0] == b"tmFunction":
                header = struct.unpack_from(">6I", dat, 32 + offset)
        self.assertIsNotNone(header)
        code, table, count, exports, num_exports, size = header
        self.base = 0x80700000
        payload = bytearray(dat[32 + code:32 + code + size])
        for i in range(count):
            word, target = struct.unpack_from(">II", dat, 32 + table + i * 8)
            kind, offset = word >> 24, word & 0xFFFFFF
            # Native loader treats 0x8... targets as absolute, others as offsets.
            if target & 0xF0000000 != 0x80000000:
                target = (target + self.base) & 0xFFFFFFFF
            if kind == 1:
                struct.pack_into(">I", payload, offset, target)
            elif kind == 4:
                struct.pack_into(">H", payload, offset, target & 0xFFFF)
            elif kind == 6:
                struct.pack_into(">H", payload, offset, ((target + 0x8000) >> 16) & 0xFFFF)
            elif kind == 10:
                old = struct.unpack_from(">I", payload, offset)[0]
                branch = (target - self.base - offset) & 0x03FFFFFC
                struct.pack_into(">I", payload, offset, old | branch)
            elif kind == 26:
                struct.pack_into(">I", payload, offset, (target - self.base - offset) & 0xFFFFFFFF)
            else:
                self.fail(f"Unknown native MEX relocation {kind}")
        self.m.cpu.mem_write(self.base, bytes(payload))
        for i in range(num_exports):
            index, offset = struct.unpack_from(">II", dat, 32 + exports + i * 8)
            if index in [27, 28]:
                self.m.symbols["DAT_Get" if index == 27 else "DAT_Set"] = self.base + offset
        # Native memcpy is an external SDK dependency; use the tested PPC stub.
        address = self.m.symbols["memcpy"]
        self.m.cpu.mem_write(0x800031F4, struct.pack(">4I", 0x3D800000 | (address >> 16),
            0x618C0000 | (address & 0xFFFF), 0x7D8903A6, 0x4E800420))
        link = (ROOT / "MexTK/melee.link").read_text()
        native_memset = int(next(line.split(":")[0] for line in link.splitlines() if line.endswith(":memset")), 16)
        address = self.m.symbols["memset"]
        self.m.cpu.mem_write(native_memset, struct.pack(">4I", 0x3D800000 | (address >> 16),
            0x618C0000 | (address & 0xFFFF), 0x7D8903A6, 0x4E800420))

    def test_new_save_identity_and_repeated_reads_after_real_dat_relocation(self):
        m = self.m
        blank = bytearray(44)
        blank[4:6] = b"\1\1"
        m.put(blank)
        self.assertEqual(m.call("DAT_Get", 1, 0), 1)
        self.assertEqual(m.record()[38:40], b"TY")
        self.assertEqual(m.record()[10] >> 6, 3)
        for _ in range(10):
            self.assertEqual(m.call("DAT_Get", 1, 0), 1)
        m.call("DAT_Set", 11, 7, 1)
        self.assertEqual(m.call("DAT_Get", 11, 7), 1)
        m.call("DAT_Set", 17, 19, 1)  # Grouped Very Fast row; native physical ID 19.
        self.assertEqual(m.call("DAT_Get", 11, 1), 1)
        self.assertEqual(m.call("DAT_Get", 17, 19), 1)
        before = m.record()
        m.call("DAT_Set", 17, 16, 1)  # First gap is not a saved preference.
        self.assertEqual(m.record(), before)

    def test_relocated_dat_migration_and_foreign_identity_preserve_records(self):
        m = self.m
        m.init()
        old = bytearray(m.record())
        old[10] = 0xBF
        m.put(old)
        self.assertEqual(m.call("DAT_Get", 11, 7), 0)
        self.assertEqual(m.record()[10] >> 6, 3)
        before = m.record()
        m.cpu.mem_write(0x80000000, b"GTME01")
        m.call("DAT_Set", 11, 7, 1)
        self.assertEqual(m.record(), before)


class SettingsTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def tearDown(self):
        self.assertEqual(bytes(self.m.cpu.mem_read(RECORD - 8, 8)), b"\xA5" * 8)
        self.assertEqual(bytes(self.m.cpu.mem_read(RECORD + 44, 8)), b"\xA5" * 8)

    def test_defaults_and_all_overlay_slots(self):
        m = self.m
        m.init()
        self.assertEqual(m.record()[38:40], b"TY")
        for group in range(18):
            m.write(12, group, group % 11)
            m.write(13, group, (group + 3) % 11)
        for group in range(18):
            self.assertEqual(m.read(12, group), group % 11)
            self.assertEqual(m.read(13, group), (group + 3) % 11)
        self.assertEqual(m.record()[40:44], b"\0" * 4)

    def test_global_trail_rows_do_not_use_enable_mask_bits(self):
        m = self.m
        m.init()
        m.write(0, 0, (1 << 2) | (1 << 4))
        self.assertEqual(m.read(16, 2), 0)
        self.assertEqual(m.read(16, 4), 0)
        m.write(16, 2, 1)
        m.write(16, 4, 1)
        self.assertEqual(m.read(11, 1), 1)
        self.assertEqual(m.read(11, 2), 1)
        self.assertEqual(m.read(0), (1 << 2) | (1 << 4))
        self.assertEqual(m.call("TestSettingsRow", 2), 1)
        self.assertEqual(m.call("TestSettingsRow", 4), 1)
        m.write(16, 2, 0)
        self.assertEqual(m.read(16, 4), 1)

    def test_global_rows_round_trip_all_four_states(self):
        for vf in (0, 1):
            for instant in (0, 1):
                saved = Machine()
                saved.init()
                saved.write(14, IDS[4], 7)
                saved.write(0, 0, saved.read(0) | (1 << 31))
                saved.write(16, 2, vf)
                saved.write(16, 4, instant)
                restored = Machine()
                restored.put(saved.record())
                self.assertEqual(restored.call("TestSettingsRow", 2), vf)
                self.assertEqual(restored.call("TestSettingsRow", 4), instant)
                self.assertEqual(restored.record(), saved.record())
                self.assertEqual(restored.read(14, IDS[4]), 7)
                self.assertEqual(restored.read(15, 31), 1)
                self.assertEqual(restored.call("TestDirty"), 0)

    def test_editing_one_global_row_preserves_sibling_and_other_settings(self):
        m = self.m
        m.init()
        m.write(14, IDS[4], 7)
        m.write(12, 16, 10)
        m.write(11, 5, 1)
        m.call("Settings_Set", 16, 4, 1)
        before = m.record()
        m.call("Settings_Set", 16, 2, 1)
        changed = m.record()
        self.assertEqual(changed[:10] + changed[11:], before[:10] + before[11:])
        self.assertEqual(m.read(16, 4), 1)
        self.assertEqual(m.read(11, 5), 1)
        m.call("Settings_Set", 15, IDS[2], 1)
        self.assertEqual(m.read(16, 2), 1)
        self.assertEqual(m.read(16, 4), 1)
        self.assertEqual(m.read(14, IDS[4]), 7)
        self.assertEqual(m.read(12, 16), 10)

    def test_colors_flags_controls_and_unknown_mask_bits(self):
        m = self.m
        m.init()
        m.write(0, 0, 1 << 31)
        for slot, osd in enumerate(IDS):
            m.write(14, osd, slot % 8)
        for slot, osd in enumerate(IDS):
            self.assertEqual(m.read(14, osd), slot % 8)
            self.assertEqual(m.read(15, osd), int(slot % 8 != 0))
        self.assertEqual(m.read(15, 31), 1)
        for flag in range(8):
            m.write(11, flag, 1)
        self.assertEqual(m.record()[10], 0xFF)
        self.assertEqual(m.record()[40] & 1, 1)
        m.write(4, 0, 4)
        m.write(5, 0, 5)
        self.assertEqual(m.record()[7], 0x54)
        m.write(15, IDS[2], 1)
        self.assertEqual(m.read(14, IDS[2]), 2)  # Boolean writer preserves an enabled Red choice.
        for field, index, value in [(4, 0, 5), (5, 0, 6), (11, 8, 1), (12, 18, 1), (14, 64, 1), (15, 32, 1)]:
            before = m.record()
            self.assertEqual(m.write(field, index, value), 0)
            self.assertEqual(m.record(), before)

    def test_legacy_migration_before_overwrite_and_idempotence(self):
        m = self.m
        legacy = bytearray(44)
        mask = (1 << 31) | (1 << IDS[4]) | 1
        legacy[:4] = mask.to_bytes(4, "big")
        legacy[4:12] = bytes([3, 2, 1, 0x54, 0x21, 0x11, 0xFF, 3])
        legacy[12:18] = bytes([0, 2, 16, 10, 0, 4])  # Duplicate group: last valid pair wins.
        legacy[28:34] = bytes([16, 9, 4, 8, 250, 250])
        m.put(legacy)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
        self.assertEqual(m.read(0), mask)
        self.assertEqual(m.read(12, 0), 4)
        self.assertEqual(m.read(12, 16), 10)
        self.assertEqual(m.read(13, 16), 9)
        self.assertEqual(m.read(13, 4), 8)
        self.assertEqual(m.record()[10], 0xC0)
        self.assertEqual(m.record()[4:10], legacy[4:10])
        self.assertEqual(m.read(14, IDS[4]), 1)
        first = m.record()
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        self.assertEqual(m.record(), first)

    def test_repair_preserves_reserve_and_unassigned_palette_bits(self):
        m = self.m
        m.init()
        r = bytearray(m.record())
        r[4:10] = b"\xFF" * 6
        r[11:30] = b"\xFF" * 19
        r[37] = 0xFE
        r[40:44] = b"KEEP"
        m.put(r)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 2)
        self.assertEqual(m.record()[4:6], b"\1\1")
        self.assertEqual(m.record()[7:10], b"\0\0\0")
        self.assertEqual(m.record()[12:30], b"\0" * 18)
        self.assertEqual(m.record()[37] & 0xFE, 0xFE)
        self.assertEqual(m.record()[40:44], b"KEEP")

    def test_foreign_and_future_records_are_not_modified(self):
        m = self.m
        m.init()
        future = bytearray(m.record())
        future[10] = 0x3F
        future[12:38] = bytes(range(26))
        m.put(future)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 3)
        self.assertEqual(m.write(0, 0, 0xFFFFFFFF), 0)
        self.assertEqual(m.record(), future)
        self.assertEqual(m.call("Settings_Get", 0, 0), 0)
        m.call("Settings_Set", 15, IDS[0], 1)
        self.assertEqual(m.call("Settings_Get", 15, IDS[0]), 1)
        self.assertEqual(m.call("TestDirty"), 0)
        self.assertEqual(m.record(), future)
        for identity in (b"GTME01", b"GALE01", b"TYRE99"):
            m.cpu.mem_write(0x80000000, identity)
            self.assertEqual(m.call("Settings_Status"), 4)
            m.call("Settings_Set", 12, 0, 5)
            self.assertEqual(m.record(), future)

    def test_random_legacy_data_stays_bounded_and_valid(self):
        m = self.m
        rng = random.Random(721)
        for _ in range(50):
            legacy = bytearray(rng.randbytes(44))
            legacy[38:40] = b"\0\0"
            m.put(legacy)
            self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
            self.assertEqual(m.read(0), int.from_bytes(legacy[:4], "big"))
            for actor, field in [(0, 12), (1, 13)]:
                expected = [0] * 18
                for pair in range(8):
                    group, choice = legacy[12 + actor * 16 + pair * 2:14 + actor * 16 + pair * 2]
                    if group < 17 and 0 < choice < 11:
                        expected[group] = choice
                for group in range(18):
                    self.assertEqual(m.read(field, group), expected[group])
            self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)

    def test_native_hook_preserves_registers(self):
        m = self.m
        m.init()
        m.write(0, 0, 0x81234567)
        m.call("TestSeedXER")
        xer = m.call("TestReadXER")
        for n in range(32):
            if n not in (1, 2):
                m.cpu.reg_write(gpr(n), 0x12340000 + n)
        before = {n: m.cpu.reg_read(gpr(n)) for n in range(32)}
        m.cpu.reg_write(reg.UC_PPC_REG_CR, 0xABCD1234)
        m.cpu.reg_write(reg.UC_PPC_REG_CTR, 0x12345678)
        fp_regs = [getattr(reg, f"UC_PPC_REG_FPR{n}") for n in range(14)]
        for n, fp in enumerate(fp_regs):
            m.cpu.reg_write(fp, 0x3FF0000000000000 + n)
        m.cpu.mem_write(0x80300000 - 200 + 27 * 4, struct.pack(">I", m.symbols["TestClobberGet"]))
        m.call("TestSettingsRead")
        self.assertEqual(m.cpu.reg_read(gpr(4)), 0x81234567)
        for n in range(32):
            if n != 4:
                self.assertEqual(m.cpu.reg_read(gpr(n)), before[n], f"r{n}")
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR), 0xABCD1234)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CTR), 0x12345678)
        self.assertEqual(m.call("TestReadXER"), xer)
        for n, fp in enumerate(fp_regs):
            self.assertEqual(m.cpu.reg_read(fp), 0x3FF0000000000000 + n)
        m.call("TestSettingsWrite", IDS[5], 1)
        self.assertEqual(m.call("Settings_Get", 15, IDS[5]), 1)
        self.assertEqual(m.call("TestDirty"), 1)


class TrailTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.bank = 0x80401000
        self.sample = 0x80403000
        self.m.cpu.mem_write(self.bank - 8, b"\xA5" * (5132 + 16))
        self.m.call("TMTrail_Clear", self.bank)

    def tearDown(self):
        self.assertEqual(bytes(self.m.cpu.mem_read(self.bank - 8, 8)), b"\xA5" * 8)
        self.assertEqual(bytes(self.m.cpu.mem_read(self.bank + 5132, 8)), b"\xA5" * 8)

    def test_decay_lifetimes_and_soft_history(self):
        for mode, lifetime in enumerate([65, 35, 21, 1, 130]):
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, 0), 200)
            for age in range(1, lifetime):
                alpha = self.m.call("TMTrail_Alpha", mode, age)
                self.assertGreater(alpha, 0)
                self.assertLessEqual(alpha, 72)
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, lifetime), 0)
        self.assertEqual(self.m.call("TMTrail_Alpha", 5, 0xFFFFFFFF), 72)
        self.assertEqual(self.m.call("TMTrail_Alpha", 99, 0), 0)

    def test_union_global_priority_and_player_palette(self):
        self.assertEqual(self.m.call("TMTrail_Effective", 1, 1, 1, 5), 2)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 1, 1, 5), 3)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 1, 3), 3)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 0, 0), 6)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 1, 99), 6)
        for index, color in enumerate([0xFF4646C8, 0x4691FFC8, 0xFFE141C8, 0x4BE164C8]):
            self.assertEqual(self.m.call("TMTrail_PlayerColor", index, 0), color)
            self.assertEqual(self.m.call("TMTrail_PlayerColor", index, 1), 0xB4B4B4C8)
        self.assertEqual(self.m.call("TMTrail_PlayerColor", 99, 0), 0xB4B4B4C8)

    def add(self, frame, source=1, x=1.0):
        sample = struct.pack(">7f3I", x, 2, 3, 4, 5, 6, 2, 0xFF4646C8, frame, source)
        self.m.cpu.mem_write(self.sample, sample)
        self.m.call("TMTrail_Add", self.bank, self.sample)

    def next_index(self):
        return struct.unpack(">I", self.m.cpu.mem_read(self.bank + 5120, 4))[0]

    def test_pause_duplicate_capture_and_timeline_reset(self):
        self.assertEqual(self.m.call("TMTrail_BeginFrame", self.bank, 10), 1)
        self.add(10)
        self.assertEqual(self.m.call("TMTrail_BeginFrame", self.bank, 10), 0)
        self.assertEqual(self.next_index(), 1)
        self.assertEqual(self.m.call("TMTrail_BeginFrame", self.bank, 11), 1)
        self.add(11)
        self.assertEqual(self.next_index(), 1)  # Stationary/hitlag sample is refreshed, not stacked.
        self.add(11, source=2)
        self.assertEqual(self.next_index(), 2)
        self.m.call("TMTrail_BeginFrame", self.bank, 5)
        self.assertEqual(self.next_index(), 0)
        self.add(5)
        self.m.call("TMTrail_BeginFrame", self.bank, 20)
        self.assertEqual(self.next_index(), 0)

    def test_ring_wrap_is_bounded(self):
        for source in range(140):
            self.add(1, source)
        self.assertEqual(self.next_index(), 12)

    def test_damage_hue_keeps_equal_opacity_and_distinct_phase_history(self):
        m = self.m
        palettes = [(0xFF4646C8,0xFF00FFC8), (0xFFE141C8,0xFF8800C8),
                    (0x4691FFC8,0x00FFFFC8), (0x4BE164C8,0x39FF14C8), (0xB4B4B4C8,0xFFFFFFC8)]
        for base, accent in palettes:
            self.assertEqual(m.call("TMTrail_DamageColor", base, 0), base)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 3), base)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 15), accent)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 100), accent)
            colors = [m.call("TMTrail_DamageColor", base, d) for d in [2,3,9,12,15]]
            self.assertNotEqual(colors[2], colors[3])  # Fox nair early/late phases remain distinct.
            for color in colors:
                self.assertEqual(m.call("TMTrail_SampleAlpha", 2, 0, color), 200)
                self.assertEqual(m.call("TMTrail_SampleAlpha", 2, 1, color), 72)
                self.assertEqual(m.call("TMTrail_SampleAlpha", 2, 21, color), 0)
                self.assertEqual(m.call("TMTrail_SampleAlpha", 3, 1, color), 0)
        strong = m.call("TMTrail_DamageColor", 0x4691FFC8, 12)
        weak = m.call("TMTrail_DamageColor", 0x4691FFC8, 9)
        for frame, color in [(0, strong), (1, weak)]:
            sample = struct.pack(">7f3I", 1,2,3,4,5,6,2,color,frame,1)
            m.cpu.mem_write(self.sample, sample)
            m.call("TMTrail_Add", self.bank, self.sample)
        self.assertEqual(self.next_index(), 2)
        self.assertEqual(struct.unpack(">I", m.cpu.mem_read(self.bank + 28, 4))[0], strong)
        self.assertEqual(struct.unpack(">I", m.cpu.mem_read(self.bank + 40 + 28, 4))[0], weak)


class OSDContextTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.context, self.sample = 0x80401000, 0x80401100
        self.m.cpu.mem_write(self.context, bytes(64))

    def tick(self, frame, state=361, attack=1, air=1, shine=1, shield=0, victim=0, hitlag=0, dead=0):
        self.m.cpu.mem_write(self.sample, struct.pack(">8i", state,attack,air,shine,shield,victim,hitlag,dead))
        self.m.call("TestContextStep", self.context, frame, self.sample)

    def hl(self, category):
        return self.m.call("TMOSDContext_Hitlag", self.context, category)

    def test_shine_fastfall_lcancel_freeze_counts_and_episode_retirement(self):
        self.tick(1, state=360, hitlag=1)
        self.tick(1, state=360, hitlag=1)  # Duplicate/frame advance redraw does not add time.
        self.tick(2, state=360, hitlag=1)
        self.tick(3, state=360, hitlag=1)
        self.tick(4, state=361)
        self.assertEqual(self.hl(8), 3)
        self.assertEqual(self.hl(20), 3)
        self.tick(5, state=70, air=0, shine=0)
        self.assertEqual(self.hl(1), 3)
        self.tick(6, state=70, air=0, shine=0, hitlag=1)
        self.assertEqual(self.hl(1), 4)
        self.tick(7, state=14, air=0, shine=0)
        self.assertEqual(self.hl(16), 4)
        self.tick(8, state=25, air=1, shine=0)
        self.assertEqual(self.hl(20), 0)
        self.tick(9, state=360, hitlag=1)
        self.assertEqual(self.hl(8), 1)  # New shine excludes an old contact.
        self.tick(10, state=0, dead=1)
        self.assertEqual(self.hl(20), 0)
        self.tick(11, state=361, hitlag=1)
        self.tick(2, state=361)
        self.assertEqual(self.hl(8), 0)  # Rewind clears transient counters.

    def test_shield_victim_and_other_timing_categories_use_own_episode(self):
        self.tick(1, state=179, air=0, shine=0, shield=1, hitlag=1)
        self.tick(2, state=180, air=0, shine=0, shield=1, hitlag=1)
        self.tick(3, state=178, air=0, shine=0, shield=1)
        self.assertEqual(self.hl(3), 2)
        self.tick(4, state=75, air=1, shine=0, victim=1, hitlag=1)
        self.tick(5, state=76, air=1, shine=0, victim=1, hitlag=1)
        self.tick(6, state=29, air=1, shine=0)
        self.assertEqual(self.hl(28), 2)
        self.tick(7, state=65, attack=2, shine=0, hitlag=1)
        self.tick(8, state=65, attack=2, shine=0, hitlag=1)
        self.assertEqual(self.hl(19), 2)
        self.tick(9, state=44, attack=3, air=0, shine=0)
        self.assertEqual(self.hl(19), 0)

    def test_regular_landing_actual_lag_direct_exits_and_intermediate_wait(self):
        def measure(states, frames, lag=4):
            self.m.cpu.mem_write(self.sample, struct.pack(">6H", *(states + [0] * (6-len(states)))))
            self.m.cpu.mem_write(self.sample+12, struct.pack(">6H", *(frames + [0] * (6-len(frames)))))
            result = self.m.call("TMOSD_WaitFrames", self.sample,self.sample+12,6,lag,self.sample+24,self.sample+28)
            return result, struct.unpack(">2i", self.m.cpu.mem_read(self.sample+24,8))
        self.assertEqual(measure([42,25],[4,20]), (1,(0,0)))  # Ordinary jump landing now qualifies.
        self.assertEqual(measure([42,25],[6,20],6)[0], 1)
        self.assertEqual(measure([42,65],[7,20],4)[0], 4)
        self.assertEqual(measure([14,42,25],[3,4,20])[0], 3)
        self.assertEqual(measure([18,14,70],[1,2,10])[0], 2)
        self.assertEqual(measure([70,65],[20,20])[0], 1)  # Direct aerial lag exit.
        self.assertEqual(measure([43,236],[10,20])[0], 1)  # Direct waveland exit.
        self.assertEqual(measure([42,25],[17,20])[0], 0)  # Retain existing 13-opportunity display policy.

    def test_native_adapter_player_subfighter_lifetimes_freeze_and_wait_source(self):
        m = self.m
        m.init(); m.call("TestCueInit")
        m.call("TestCueKind", 0, 2)
        m.call("TestCueState", 0, 360, 0, 40, 100)
        data = m.call("TestCueData", 0)
        # phys.air_state is not needed for ground-shine episode.
        m.call("TestCueFrozen", 0, 1)
        for frame in range(1,4): m.call("TestContextTick", 0, frame)
        self.assertEqual(m.call("OSDContext_MessageHitlag", 0, 8), 3)
        self.assertEqual(m.call("OSDContext_MessageHitlag", 6, 8), 0)
        m.call("TestCueSpawn", 0, 2, 0)
        self.assertEqual(m.call("OSDContext_MessageHitlag", 0, 8), 0)
        m.call("TestCueFrozen", 0, 0)
        m.call("TestCueState", 0, 44, 0, 40, 100)
        m.cpu.mem_write(data+0x23F8, b"\0\0")
        m.cpu.mem_write(data+0x23FC, struct.pack(">6H",42,25,0,0,0,0))
        m.cpu.mem_write(data+0x2408, struct.pack(">6H",4,20,0,0,0,0))
        m.call("TestCueLanding", 0, 4, 1)
        m.call("TestWaitDisplay", 0)
        self.assertEqual(m.call("TestWaitFrame"), 1)
        self.assertEqual(bytes(m.call("TestWaitLabelChar", i) for i in range(7)), b"Landing")
        self.assertEqual(m.call("OSD_MessageSettings", m.call("TestWaitTag")), 16)
        m.call("TestCueState", 0, 70, 0, 40, 100)
        m.call("TestCueMissed", 0, 6)
        m.call("TestCueState", 0, 44, 0, 40, 100)
        m.cpu.mem_write(data+0x23FC, struct.pack(">6H",14,70,65,0,0,0))
        m.cpu.mem_write(data+0x2408, struct.pack(">6H",1,10,20,0,0,0))
        m.call("TestWaitDisplay", 0)
        self.assertEqual(bytes(m.call("TestWaitLabelChar", i) for i in range(8)), b"L-cancel")
        m.call("ActionCues_Clear")
        self.assertEqual(m.call("OSDContext_MessageHitlag", 0, 8), 0)


class OSDStyleTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def test_wavedash_compact_top_row_and_neutral_hitlag_prefix(self):
        m = self.m; m.init()
        m.write(14, 0, 1)
        m.call("TestStyleInit", 0, 2, 0, 1)
        m.call("TestStyleFormat", 0, 1, -30)
        self.assertEqual(m.call("TestStyleScale100", 0), 70)
        self.assertEqual(m.call("TestStyleScale100", 3), 70)
        self.assertEqual(m.call("TestStyleX",0), 0xFFFFFFBA)
        self.assertEqual(m.call("TestStyleX",3), 55)
        m.call("TestStyleInit", 0, 2, 0, 1)
        m.call("TestStyleFormat", 3, 1, -30)
        self.assertEqual(m.call("TestStyleScale100", 3), 55)
        self.assertEqual(m.call("TestStyleY", 3), 0xFFFFFFE2)
        self.assertEqual(m.call("TestStyleY", 4), 0xFFFFFFE2)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleColor", 3), 0x8DFF6EFF)
        self.assertEqual(m.call("TestStyleColor", 4), 0xFFFFFFFF)

    def test_lcancel_prefixed_result_keeps_window_outcome_and_measurement(self):
        m = self.m; m.init()
        m.write(14, 1, 1)
        m.call("TestStyleInit", 1, 4, 1, 1)
        m.call("TestStyleFormat", 7, 0, -30)
        self.assertEqual(bytes(m.call("TestStyleStringChar",1,i) for i in range(10)), b"Frame %d/7")
        self.assertEqual(bytes(m.call("TestStyleStringChar",3,i) for i in range(7)), b"%dhl ->")
        self.assertEqual(m.call("TestStyleTimingLine"), 1)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleColor", 1), 0xFFA2BAFF)
        self.assertEqual(m.call("TestStyleColor", 3), 0xFFFFFFFF)

    def test_message_identity_and_legacy_contract(self):
        for raw, owner in [(7, 20), (5, 16), (5, 28), (-1, 10), (13, 22), (13, 26), (64, 8)]:
            tag = self.m.call("OSD_MessageTag", raw, owner, 1, 1, 0)
            self.assertEqual(self.m.call("OSD_MessageKind", tag), raw & 0xFFFFFFFF)
            self.assertEqual(self.m.call("OSD_MessageSettings", tag), owner)
            self.assertEqual(self.m.call("OSD_MessageArgument", tag), 1)
        self.assertEqual(self.m.call("OSD_MessageSettings", -1), 0xFFFFFFFF)
        self.assertEqual(self.m.call("OSD_MessageSettings", 5), 0xFFFFFFFF)
        self.assertEqual(self.m.call("OSD_SameReplacement", 5, 16, 5, 28), 0)
        self.assertEqual(self.m.call("OSD_SameReplacement", 13, 22, 13, 26), 0)
        self.assertEqual(self.m.call("OSD_SameReplacement", 64, 8, 8, 8), 0)
        self.assertEqual(self.m.call("OSD_SameReplacement", 5, 16, 5, 16), 1)
        self.assertEqual(self.m.call("OSD_SameReplacement", -1, 10, -1, 10), 0)

    def test_timing_and_palette_colors(self):
        for frame, expected in [(0, 0xFFA2BAFF), (1, 0x00FFFFFF), (2, 0x8DFF6EFF), (3, 0xFFFFFFFF), (4, 0xFFA2BAFF), (99, 0xFFA2BAFF)]:
            self.assertEqual(self.m.call("OSD_TimingColor", frame), expected)
        expected = [0, 0xFFFFFFFF, 0xFF4646FF, 0x8DFF6EFF, 0x4691FFFF, 0xFFF000FF, 0x00FFFFFF, 0xFF50FFFF]
        for index, rgba in enumerate(expected):
            self.assertEqual(self.m.call("OSD_PaletteColor", index), rgba)
        self.assertEqual(self.m.call("OSD_PaletteColor", 99), 0xFFFFFFFF)

    def test_non_one_best_frame_does_not_change_measurement(self):
        tag = self.m.call("OSD_MessageTag", 8, 8, 1, 1, 0) | (4 << 23)
        self.assertEqual(self.m.call("OSD_MessageBestFrame", tag), 5)
        self.assertEqual(self.m.call("TestTimingArgument", tag, 0, 0, 0, 5), 5)
        for frame, color in [(4, 0xFFA2BAFF), (5, 0x00FFFFFF), (6, 0x8DFF6EFF), (7, 0xFFFFFFFF), (8, 0xFFA2BAFF)]:
            self.assertEqual(self.m.call("OSD_TimingColorFor", frame, 5), color)

    def test_draw_applies_late_title_and_best_frame_colors(self):
        m = self.m
        m.init()
        m.write(14, 8, 7)
        m.call("TestStyleInit", 8, 5, 1, 5)
        m.call("TestStyleCallerColor", 0, 0x8DFF6EFF)
        m.call("TestStyleCallerColor", 1, 0xFFA2BAFF)
        m.call("TestStyleCallerColor", 2, 0x123456FF)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleColor", 0), 0xFF50FFFF)
        self.assertEqual(m.call("TestStyleColor", 1), 0x00FFFFFF)
        self.assertEqual(m.call("TestStyleColor", 2), 0x123456FF)
        self.assertEqual(m.call("TestStyleHidden"), 0)

    def test_off_and_event_owned_messages_keep_safe_objects(self):
        m = self.m
        m.init()
        m.call("TestStyleInit", 20, 2, 1, 1)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 1)
        self.assertEqual(m.call("TestStyleBackgrounds"), 0)
        m.write(14, 20, 4)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 0)
        self.assertEqual(m.call("TestStyleBackgrounds"), 1)
        m.call("TestStyleInit", -1, -1, 1, 1)
        m.call("TestStyleCallerColor", 0, 0x112233FF)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 0)
        self.assertEqual(m.call("TestStyleColor", 0), 0x112233FF)

    def test_native_tag_and_vararg_snapshot(self):
        native_tag = self.m.call("TestOSDTag", 7)
        c_tag = self.m.call("OSD_MessageTag", 7, 20, 1, 1, 0)
        self.assertEqual(native_tag, c_tag)
        self.assertEqual(self.m.call("TestTimingArgument", native_tag, 0, 0, 0, 3), 3)
        lc = self.m.call("OSD_MessageTag", 1, 1, 2, 1, 0)
        self.assertEqual(self.m.call("TestTimingArgument", lc, 0, 0, 0, 75, 2), 2)
        no_time = self.m.call("OSD_MessageTag", 9, 9, 0, 1, 0)
        self.assertEqual(self.m.call("TestTimingArgument", no_time, 0, 0, 0, 123), 0xFFFFFFFF)
        wave = self.m.call("OSD_MessageTag", 0, 0, 1, 0, 1)
        self.assertEqual(self.m.call("OSD_MessageInline", wave), 1)
        self.assertEqual(self.m.call("OSD_MessageLine", wave), 0)

    def test_master_off_hides_existing_messages_and_restores_choices(self):
        m = self.m
        m.init()
        m.write(14, 20, 6)
        m.write(11, 1, 1)
        before = m.record()
        m.call("TestStyleInit", 20, 2, 1, 1)
        m.call("TestStyleDraw")
        m.write(16, 6, 1)
        self.assertEqual(m.read(16, 6), 1)
        self.assertEqual(m.read(15, 6), 0)  # Master is not an OSD mask bit.
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 1)
        self.assertEqual(m.call("TestStyleBackgrounds"), 1)
        self.assertEqual(m.read(11, 1), 1)  # Trails remain On.
        m.call("TestStyleInit", -1, -1, 1, 1)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 0)
        m.write(16, 6, 0)
        self.assertEqual(m.record(), before)
        m.call("TestStyleInit", 20, 2, 1, 1)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"), 0)
        self.assertEqual(m.call("TestStyleColor", 0), 0x00FFFFFF)


class OSDEditorTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()

    def test_native_palette_cycle_and_exit_snapshot(self):
        m = self.m
        m.write(14, 20, 6)
        m.write(15, 15, 1)  # Reserved event bit must survive.
        m.call("TestEditorInit")
        self.assertEqual(m.call("TestEditorChoiceChar", 13, 0), ord("C"))
        self.assertEqual(m.call("TestEditorColor", 13), 0x00FFFFFF)
        self.assertEqual(m.call("TestEditorInput", 0x200, 13), 1)
        self.assertEqual(m.read(14, 20), 7)
        self.assertEqual(m.call("TestEditorChoiceChar", 13, 0), ord("M"))
        self.assertEqual(m.call("TestEditorInput", 0x200, 13), 1)
        self.assertEqual(m.read(14, 20), 0)
        self.assertEqual(m.call("TestEditorCache", 13), 1)  # Native detects the changed Boolean.
        self.assertEqual(bytes(m.cpu.mem_read(0x804A04F4, 1)), b"\0")
        m.call("TestEditorAnimate", 13)
        self.assertEqual(m.call("TestEditorCache", 13), 0)
        self.assertEqual(m.call("TestEditorInput", 0x10, 13), 1)
        self.assertEqual(m.read(14, 20), 7)
        self.assertEqual(m.call("TestEditorCache", 13), 0)
        self.assertEqual(bytes(m.cpu.mem_read(0x804A04F4, 1)), b"\1")
        m.call("TestEditorAnimate", 13)
        self.assertEqual(m.call("TestEditorCache", 13), 1)
        # The native exit pass saves Boolean rows: chosen Magenta remains Magenta.
        m.write(16, 20, m.call("TestEditorCache", 13))
        self.assertEqual(m.read(14, 20), 7)
        self.assertEqual(m.read(15, 15), 1)

    def test_master_and_trails_are_independent_boolean_rows(self):
        m = self.m
        for id in IDS:
            m.write(14, id, (id % 7) + 1)
        before = m.record()
        m.call("TestEditorInit")
        for row, flag in [(22, 1), (23, 2), (20, 0)]:
            m.call("TestEditorInput", 0x200, row)
            self.assertEqual(m.read(11, flag), 1)
            self.assertEqual(m.call("TestEditorChoiceChar", row, 1), ord("n"))
        self.assertEqual(m.record()[:10], before[:10])
        self.assertEqual(m.record()[11:], before[11:])
        self.assertEqual(m.read(11, 1), 1)
        self.assertEqual(m.read(11, 2), 1)
        m.call("TestEditorInput", 0x10, 20)
        self.assertEqual(m.read(11, 0), 0)
        self.assertEqual(m.read(11, 1), 1)
        self.assertEqual(m.read(11, 2), 1)

    def test_unused_rows_and_navigation_do_not_edit_settings(self):
        m = self.m
        m.call("TestEditorInit")
        before = m.record()
        for row in [19, 21, 29, 65535]:
            self.assertEqual(m.call("TestEditorInput", 0x200, row), 1)
            self.assertEqual(m.call("TestEditorInput", 0x10, row), 1)
        for button in [1, 2, 4, 8, 0x100, 0x400, 0x800, 0x1000]:
            self.assertEqual(m.call("TestEditorInput", button, 0), 0)
        self.assertEqual(m.record(), before)

    def test_native_editor_future_format_uses_private_defaults(self):
        m = self.m
        future = bytearray(m.record())
        future[10] = 0
        m.put(future)
        m.call("TestEditorInit")
        m.call("TestEditorInput", 0x200, 13)
        self.assertEqual(m.call("Settings_Get", 14, 20), 1)
        m.call("TestEditorInput", 0x200, 20)
        self.assertEqual(m.call("Settings_Get", 11, 0), 1)
        self.assertEqual(m.record(), bytes(future))

    def test_native_palette_and_master_round_trip_into_fresh_service(self):
        m = self.m
        m.call("TestEditorInit")
        for _ in range(6):
            m.call("TestEditorInput", 0x200, 13)
        m.call("TestEditorAnimate", 13)
        m.call("TestEditorInput", 0x200, 20)
        m.call("TestEditorAnimate", 20)
        m.write(16, 20, m.call("TestEditorCache", 13))
        m.write(16, 6, m.call("TestEditorCache", 20))
        fresh = Machine()
        fresh.put(m.record())
        fresh.call("TestEditorInit")
        self.assertEqual(fresh.call("Settings_Get", 14, 20), 6)
        self.assertEqual(fresh.call("Settings_Get", 11, 0), 1)
        self.assertEqual(fresh.call("TestEditorChoiceChar", 13, 0), ord("C"))
        fresh.call("TestEditorInput", 0x200, 20)
        self.assertEqual(fresh.call("Settings_Get", 14, 20), 6)
        self.assertEqual(fresh.call("Settings_Get", 11, 0), 0)

    def test_grouped_editor_gaps_and_native_exit_keep_every_preference(self):
        m = self.m
        physical = [0,1,2,3,4,5,6,7,8,9,11,10,24,26,28,12,13,14,15,16,17,18,19,20,21,22,23,25,27]
        expected = IDS + [255,6,255,2,4,7,17,11,23,25]
        for i, id in enumerate(IDS):
            m.write(14, id, (i % 7) + 1)
        for flag in range(8):
            m.write(11, flag, flag & 1)
        m.write(15, 15, 1)  # Reserved event preference survives menu exit.
        m.call("TestEditorInit")
        for row, native in enumerate(physical):
            self.assertEqual(m.call("TMSettings_EditorID", native), expected[row])
            self.assertEqual(m.call("TestEditorHidden", row), int(row in [19, 21]))
            self.assertEqual(m.read(17, native), m.call("TestEditorCache", row))
            self.assertEqual(m.call("TestSettingsEditorRow", native), m.call("TestEditorCache", row))
        label = "OVERRIDE OSDS OFF"
        self.assertEqual(''.join(chr(m.call("TestEditorLabelChar", 20, i)) for i in range(len(label))), label)
        m.call("TestEditorInput", 0x200, 7)  # Lockout Timers palette.
        m.call("TestEditorAnimate", 7)
        m.call("TestEditorInput", 0x200, 25)  # Run Turnaround flag.
        m.call("TestEditorAnimate", 25)
        before = m.record()
        for row, native in enumerate(physical):
            m.call("TestSettingsEditorWrite", native, m.call("TestEditorCache", row))
        self.assertEqual(m.record(), before)
        self.assertEqual(m.read(15, 15), 1)
        self.assertEqual(m.call("TestEditorLabelChar", 19, 0), 0)
        self.assertEqual(m.call("TestEditorLabelChar", 21, 0), 0)


class ActionCueTests(unittest.TestCase):
    YELLOW, GREEN, RED = 0xFFF000DC, 0x50FF5ADC, 0xFF2828B4

    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.call("TestCueInit")
        self.m.call("Settings_Set", 11, 4, 1)

    def show(self, tick, state, frame, end=40, rate=100, slot=0):
        m = self.m
        m.call("TestCueState", slot, state, frame, end, rate)
        m.call("TestCueTick", tick)
        m.call("TestCueDraw", slot)
        return m.call("TestCueDrawColor", slot)

    def test_normal_and_autocancel_landing_two_yellow_two_green(self):
        m = self.m
        m.call("TestCueLanding", 0, 5, 1)
        results = [self.show(i, 42, i) for i in range(8)]
        self.assertEqual(results, [0, 0, 0, self.YELLOW, self.YELLOW, self.GREEN, self.GREEN, 0])

    def test_wavelanding_uses_actual_rate_and_end_not_normal_lag(self):
        m = self.m
        m.call("TestCueLanding", 0, 4, 0)
        results = [self.show(i, 43, i * 2, 19, 200) for i in range(10)]
        self.assertEqual(results[-3:], [0, self.YELLOW, self.YELLOW])
        self.assertEqual(self.show(10, 14, 0), self.GREEN)
        self.assertEqual(self.show(11, 14, 1), self.GREEN)
        self.assertEqual(self.show(12, 14, 2), 0)

    def test_protected_perfect_waveland_and_local_highlight_use_native_selected_slot(self):
        m = self.m
        data = m.call("TestCueData",0)
        m.cpu.mem_write(data+0x430, struct.pack(">I",9))  # Native protected colanim selects slot 0.
        m.call("TestCueProtection",0,0,2,0)
        m.call("Settings_Set",11,7,1)
        m.call("TestCueLanding",0,4,0)
        self.assertEqual(self.show(1,43,8,10),self.YELLOW)
        self.assertEqual(self.show(2,43,9,10),self.YELLOW)
        self.assertEqual(self.show(3,14,0),self.GREEN)
        self.assertEqual(self.show(4,14,1),self.GREEN)
        self.assertEqual(self.show(5,14,2),0xFF464670)
        self.assertEqual(struct.unpack(">I",m.cpu.mem_read(data+0x430,4))[0],9)
        for flag in range(8): m.call("Settings_Set",11,flag,0)
        m.call("TestCueLocal",0,0xEEBB44E6)
        m.call("TestCueDraw",0)
        self.assertEqual(m.call("TestCueDrawColor",0),0xEEBB44E6)
        self.assertEqual(m.call("TestCueDrawFlags",0),4)
        self.assertEqual(struct.unpack(">I",m.cpu.mem_read(data+0x430,4))[0],9)

    def test_all_five_landing_lags_cancelled_and_uncancelled(self):
        for state in range(70, 75):
            for rate, frames in [(100, [6, 7]), (200, [4, 6])]:
                self.m.call("ActionCues_Clear")
                self.assertEqual(self.show(1, state, frames[0], 8, rate), self.YELLOW)
                self.assertEqual(self.show(2, state, frames[1], 8, rate), self.YELLOW)
                self.assertEqual(self.show(3, 14, 0), self.GREEN)
                self.assertEqual(self.show(4, 14, 1), self.GREEN)
                self.assertEqual(self.show(5, 14, 2), 0)

    def test_aerials_and_grounded_attacks_use_iasa_before_animation_end(self):
        m = self.m
        script = 0x80403000
        m.cpu.mem_write(script, struct.pack(">II", 22 << 26, 0))
        for state in [44, 45, 46, 49, 50, 51, 55, 57, 60, 63, 64, 65, 66, 67, 68, 69]:
            m.call("ActionCues_Clear")
            m.call("TestCueIASA", 0, 0)
            m.call("TestCueScript", 0, script, 200, 1000)
            self.assertEqual(self.show(1, state, 10, 80), self.YELLOW)
            m.call("TestCueScript", 0, script, 100, 1100)
            self.assertEqual(self.show(2, state, 11, 80), self.YELLOW)
            m.call("TestCueIASA", 0, 1)
            self.assertEqual(self.show(3, state, 12, 80), self.GREEN)
            self.assertEqual(self.show(4, state, 13, 80), self.GREEN)
            self.assertEqual(self.show(5, state, 14, 80), 0)

    def test_green_completion_pulse_survives_immediate_next_action(self):
        self.assertEqual(self.show(1, 65, 38, 40), self.YELLOW)
        self.assertEqual(self.show(2, 65, 39, 40), self.YELLOW)
        self.assertEqual(self.show(3, 65, 0, 40), 0)  # Same-state rewind clears history.
        self.m.call("ActionCues_Clear")
        self.assertEqual(self.show(4, 70, 7, 8), self.YELLOW)
        self.assertEqual(self.show(5, 20, 0), self.GREEN)  # Dash on first recovered frame.
        self.assertEqual(self.show(6, 44, 0), self.GREEN)  # Next action does not shorten confirmation.
        self.assertEqual(self.show(7, 44, 1), 0)

    def test_timing_replaces_local_red_and_restores_all_native_colanim_bytes(self):
        m = self.m
        ptr = m.call("TestCueData", 0) + 0x408
        native = bytearray((i * 17 + 9) & 255 for i in range(0x180))
        native[0x80 + 0x2C:0x80 + 0x30] = self.RED.to_bytes(4, "big")
        m.cpu.mem_write(ptr, bytes(native))
        m.call("Settings_Set", 11, 3, 1)
        m.call("TestCueState", 0, 70, 6, 8, 100)
        m.call("TestCueMissed", 0, 7)
        self.assertEqual(self.show(1, 70, 6, 8), self.YELLOW)
        self.assertEqual(m.call("TestCueDrawFlags", 0), 4)
        self.assertEqual(bytes(m.cpu.mem_read(ptr, 0x180)), bytes(native))
        self.assertEqual(self.show(2, 70, 7, 8), self.YELLOW)
        self.assertEqual(self.show(3, 14, 0), self.GREEN)
        self.assertEqual(self.show(4, 14, 1), self.GREEN)
        self.assertEqual(bytes(m.cpu.mem_read(ptr, 0x180)), bytes(native))

    def test_missed_cancel_pulses_through_landing_then_stops(self):
        m = self.m
        m.call("Settings_Set", 11, 4, 0)
        m.call("Settings_Set", 11, 3, 1)
        m.call("TestCueState", 0, 70, 0, 40, 100)
        m.call("TestCueMissed", 0, 7)
        self.assertEqual([self.show(i, 70, i) for i in range(25)],
                         [0xFF282800 | m.call("TMCue_RedAlpha", i) for i in range(25)])
        self.assertEqual(self.show(25, 14, 0), 0)
        m.call("ActionCues_Clear")
        m.call("TestCueMissed", 0, 6)
        self.assertEqual(self.show(6, 70, 0), 0)

    def test_turnrun_flash_is_specific_and_independent_of_text_master(self):
        m = self.m
        m.call("Settings_Set", 11, 4, 0)
        m.call("Settings_Set", 11, 6, 1)
        m.call("Settings_Set", 11, 0, 1)
        for state in [18, 20, 21, 23]:
            m.call("ActionCues_Clear")
            self.assertEqual(self.show(1, state, 0), 0)
        m.call("ActionCues_Clear")
        self.assertEqual([self.show(i, 19, i) for i in range(25)],
                         [0xFF282800 | m.call("TMCue_RedAlpha", i) for i in range(25)])
        self.assertEqual(self.show(6, 14, 0), 0)
        self.assertEqual(self.show(7, 19, 0), self.RED)
        m.call("Settings_Set", 11, 6, 0)
        m.call("Settings_Set", 11, 3, 1)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)

    def test_red_pulse_holds_phase_in_pause_hitlag_and_stops_at_state_exit(self):
        m = self.m
        m.call("Settings_Set", 11, 4, 0)
        m.call("Settings_Set", 11, 3, 1)
        m.call("TestCueState", 0, 70, 0, 40, 100)
        m.call("TestCueMissed", 0, 7)
        self.assertEqual(self.show(1, 70, 0), self.RED)
        pulse = self.show(2, 70, 1)
        self.assertEqual(pulse, 0xFF28288C)
        self.assertEqual(self.show(2, 70, 1), pulse)
        m.call("TestCueFrozen", 0, 1)
        self.assertEqual(self.show(3, 70, 1), pulse)
        self.assertEqual(self.show(4, 70, 1), pulse)
        m.call("TestCueFrozen", 0, 0)
        self.assertEqual(self.show(5, 70, 2), 0xFF282864)
        self.assertEqual(self.show(6, 14, 0), 0)
        for phase in range(8):
            self.assertEqual(m.call("TMCue_RedAlpha", phase), m.call("TMCue_RedAlpha", phase + 8))

    def test_respawn_reused_gobj_keeps_renderer_and_protection(self):
        m = self.m
        self.show(1, 14, 0)
        before = m.call("TestCueDrawCount", 0)
        m.call("TestCueSpawn", 0, 2, 1)
        self.show(2, 0, 0)
        self.assertEqual(m.call("TestCueDrawCount", 0), before + 1)
        m.call("TestCueSpawn", 0, 3, 0)
        m.call("Settings_Set", 11, 7, 1)
        m.call("TestCueProtection", 0, 0, 1, 0)
        self.assertEqual(self.show(3, 13, 0), 0xFF464670)
        self.assertEqual(m.call("TestCueDrawCount", 0), before + 2)
        for flag in range(8): m.call("Settings_Set", 11, flag, 0)
        self.show(4, 14, 0)
        self.assertEqual(m.call("TestCueWrapped", 0), 0)
        m.call("TestCueSpawn", 0, 4, 1)
        self.show(5, 0, 0)
        m.call("TestCueSpawn", 0, 5, 0)
        self.show(6, 13, 0)
        self.assertEqual(m.call("TestCueDrawCount", 0), before + 5)

    def test_lcancel_entry_fallback_uses_native_integer_window(self):
        m = self.m
        m.call("Settings_Set", 11, 4, 0)
        m.call("Settings_Set", 11, 3, 1)
        m.call("TestCueInputTimer", 0, 7)
        self.assertEqual(self.show(1, 70, 0), self.RED)
        m.call("TestCueInputTimer", 0, 100)  # Entry result stays latched.
        self.assertEqual(self.show(2, 70, 1), 0xFF28288C)
        self.show(3, 14, 0)
        m.call("TestCueInputTimer", 0, 6)
        self.assertEqual(self.show(4, 70, 0), 0)

    def test_roll_windows_and_local_overlay_restore_after_draw(self):
        m = self.m
        m.call("Settings_Set", 11, 7, 1)
        for state in [181, 182, 183]:  # Spotdodge, rolls (state-independent status check).
            m.call("TestCueProtection", 0, 0, 0, 0)
            self.assertEqual(self.show(state, state, 0), 0)
            m.call("TestCueProtection", 0, 2, 0, 0)
            m.call("TestCueDraw", 0)
            self.assertEqual(m.call("TestCueDrawColor", 0), 0xFF464670)
            m.call("TestCueProtection", 0, 0, 0, 0)
            m.call("TestCueDraw", 0)
            self.assertEqual(m.call("TestCueDrawColor", 0), 0)
        for flag in range(8): m.call("Settings_Set", 11, flag, 0)
        m.call("TestCueTick", 200)
        m.call("TestCueLocal", 0, 0xAABBCCDD)
        ptr = m.call("TestCueData", 0) + 0x408
        original = bytes(m.cpu.mem_read(ptr, 0x180))
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0xAABBCCDD)
        self.assertEqual(bytes(m.cpu.mem_read(ptr, 0x180)), original)
        m.call("TestCueLocal", 0, 0)
        self.assertEqual(m.call("TestCueWrapped", 0), 0)

    def test_protection_when_all_hurt_capsules_are_protected(self):
        m = self.m
        m.call("Settings_Set", 11, 7, 1)
        m.call("TestCueCapsules", 0, 15, 1)
        self.assertEqual(self.show(1, 182, 0), 0xFF464670)
        m.call("TestCueCapsule", 0, 4, 0)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)  # One protected limb is not whole-body protection.
        m.call("TestCueCapsules", 0, 15, 0)
        m.call("TestCueCapsule", 0, 4, 2)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)
        m.call("TestCueCapsules", 0, 15, 2)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0xFF464670)

    def test_pause_hitlag_restore_and_subfighters(self):
        m = self.m
        self.assertEqual(self.show(10, 42, 3, 5, slot=11), self.YELLOW)
        self.assertEqual(self.show(10, 42, 4, 5, slot=11), self.YELLOW)  # No extra tick on same key.
        m.call("TestCueFrozen", 11, 1)
        self.assertEqual(self.show(11, 42, 4, 5, slot=11), 0)
        m.call("TestCueFrozen", 11, 0)
        self.assertEqual(self.show(12, 42, 4, 5, slot=11), self.YELLOW)
        self.assertEqual(self.show(13, 14, 0, slot=11), self.GREEN)
        m.call("ActionCues_Clear")
        self.assertEqual(self.show(14, 14, 1, slot=11), 0)

    def test_infinite_shields_full_health_all_slots_and_off_does_not_write(self):
        m = self.m
        m.call("Settings_Set", 11, 5, 1)
        for slot in range(12):
            m.call("TestCueState", slot, 14, 0, 40, 100)
            ptr = m.call("TestCueData", slot)
            m.cpu.mem_write(ptr + 0x1998, struct.pack(">f", 8))  # SDK shield.health offset.
        m.call("TestCueTick", 1)
        for slot in range(12):
            ptr = m.call("TestCueData", slot)
            self.assertEqual(struct.unpack(">f", m.cpu.mem_read(ptr + 0x1998, 4))[0], 60)
        m.call("Settings_Set", 11, 5, 0)
        ptr = m.call("TestCueData", 0)
        m.cpu.mem_write(ptr + 0x1998, struct.pack(">f", 8))
        m.call("TestCueTick", 2)
        self.assertEqual(struct.unpack(">f", m.cpu.mem_read(ptr + 0x1998, 4))[0], 8)

    def test_version_one_migration_initializes_new_bit_and_preserves_30_reserved_bits(self):
        m = self.m
        old = bytearray(m.record())
        old[10] = 0x7F
        old[40:44] = b"\xFFABC"
        m.put(old)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
        self.assertEqual(m.record()[10], 0xFF)
        self.assertEqual(m.record()[40:44], b"\xFCABC")
        self.assertEqual(m.read(11, 6), 0)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        m.write(16, 17, 1)
        self.assertEqual(m.record()[40:44], b"\xFDABC")
        self.assertEqual(m.record()[10], 0xFF)  # Version bits do not change.
        for row, flag in [(7, 3), (11, 4), (17, 6), (23, 5)]:
            self.assertEqual(m.read(16, row), m.read(11, flag))

    def test_script_lookahead_is_bounded_read_only_and_handles_native_controls(self):
        m = self.m
        script, snapshot = 0x80403000, 0x80402000
        cases = [
            ([22 << 26, 0], 100, 1),
            ([22 << 26, 0], 200, 2),
            ([(1 << 26) | 1, 22 << 26, 0], 100, 2),
            ([(2 << 26) | 12, 22 << 26, 0], 100, 2),
            ([(10 << 26), 0, 0, 0, 0, 22 << 26, 0], 100, 1),
            ([(3 << 26) | 2, (1 << 26) | 1, 4 << 26, 22 << 26, 0], 0, 2),
            ([7 << 26, script], 0, 0xFFFFFFFF),
            ([63 << 26], 0, 0xFFFFFFFF),
        ]
        for words, timer, expected in cases:
            m.cpu.mem_write(script, struct.pack(">" + "I" * len(words), *words))
            data = struct.pack(">ffII5I", timer / 100, 10, script, 0, *([0] * 5))
            m.cpu.mem_write(snapshot, data)
            self.assertEqual(m.call("TestCueScriptIASA", snapshot, 100), expected)
            self.assertEqual(bytes(m.cpu.mem_read(snapshot, len(data))), data)
        for args, expected in [((300, 100, 500), 2), ((400, 100, 500), 1), ((500, 100, 500), 0), ((300, 0, 500), 0xFFFFFFFF)]:
            self.assertEqual(m.call("TestCueRemaining", *args), expected)

    def test_character_exceptions_and_excluded_states(self):
        m = self.m
        for kind, state, common in [(24, 347, 65), (24, 350, 70), (24, 344, 49), (4, 351, 50), (4, 352, 50)]:
            self.assertEqual(m.call("ActionCues_CommonState", kind, state), common)
            m.call("ActionCues_Clear")
            m.call("TestCueKind", 0, kind)
            self.assertEqual(self.show(1, state, 38, 40), self.YELLOW)
        m.call("TestCueKind", 0, 4)
        m.call("TestCueIASA", 0, 1)
        m.call("TestCueState", 0, 352, 10, 40, 100)
        self.assertEqual(m.call("ActionCues_Remaining", m.call("TestCueObject", 0)), 3)
        m.call("TestCueKind", 0, 1)
        for state in [47, 48, 181, 183, 199, 341]:
            m.call("TestCueState", 0, state, 38, 40, 100)
            self.assertEqual(m.call("ActionCues_Remaining", m.call("TestCueObject", 0)), 0xFFFFFFFF)

    def test_new_global_editor_rows_round_trip_and_preserve_other_choices(self):
        m = self.m
        for flag in [3, 4, 5, 6]:
            m.call("Settings_Set", 11, flag, 0)
        m.call("TestEditorInit")
        m.write(14, 20, 6)
        for cursor, row, flag in [(24, 7, 3), (26, 11, 4), (25, 17, 6), (27, 23, 5)]:
            m.call("TestEditorInput", 0x200, cursor)
            self.assertEqual(m.read(11, flag), 1)
            m.call("TestEditorAnimate", cursor)
            m.write(16, row, m.call("TestEditorCache", cursor))
        fresh = Machine()
        fresh.put(m.record())
        fresh.call("TestEditorInit")
        for cursor, flag in [(24, 3), (26, 4), (25, 6), (27, 5)]:
            self.assertEqual(fresh.call("Settings_Get", 11, flag), 1)
        self.assertEqual(fresh.read(14, 20), 6)

    def test_protection_overlay_uses_trail_palette_for_all_sources_and_cpu(self):
        m = self.m
        m.call("Settings_Set", 11, 7, 1)
        palette = [0xFF464670, 0x4691FF70, 0xFFE14170, 0x4BE16470, 0xB4B4B470]
        for source in [(0, 1), (0, 2), (1, 0), (2, 0)]:
            for accent, color in enumerate(palette):
                m.call("TestCuePlayer", 5, accent, accent == 4)
                m.call("TestCueState", 11, 181, 0, 40, 100)
                m.call("TestCueTick", 1)
                ptr = m.call("TestCueData", 11) + 0x408
                before = bytes(m.cpu.mem_read(ptr, 0x180))
                m.call("TestCueProtection", 11, *source, 0)
                m.call("TestCueDraw", 11)
                self.assertEqual(m.call("TestCueDrawColor", 11), color)
                self.assertEqual(bytes(m.cpu.mem_read(ptr, 0x180)), before)
                m.call("TestCueProtection", 11, 0, 0, 0)
                m.call("TestCueDraw", 11)
                self.assertEqual(m.call("TestCueDrawColor", 11), 0)
        m.call("TestCuePlayer", 5, 0, 1)  # CPU is Gray even with a Red accent.
        m.call("TestCueProtection", 11, 0, 1, 0)
        m.call("TestCueDraw", 11)
        self.assertEqual(m.call("TestCueDrawColor", 11), palette[4])

    def test_protection_is_live_while_paused_and_timing_has_priority(self):
        m = self.m
        m.call("Settings_Set", 11, 7, 1)
        m.call("Settings_Set", 11, 0, 1)  # Text master does not disable protection.
        m.call("TestCuePlayer", 0, 1, 0)
        m.call("TestCueProtection", 0, 0, 2, 0)
        self.assertEqual(self.show(1, 42, 3, 5), self.YELLOW)
        self.assertEqual(self.show(2, 42, 4, 5), self.YELLOW)
        self.assertEqual(self.show(3, 14, 0), self.GREEN)
        self.assertEqual(self.show(4, 14, 1), self.GREEN)
        self.assertEqual(self.show(5, 14, 2), 0x4691FF70)
        m.call("Settings_Set", 11, 7, 0)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)
        m.call("Settings_Set", 11, 7, 1)
        m.call("ActionCues_Clear")
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0x4691FF70)
        m.call("TestCueProtection", 0, 0, 0, 0)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)

    def test_yoshi_double_jump_armor_and_move_protection(self):
        m = self.m
        m.call("Settings_Set", 11, 7, 1)
        m.call("TestCuePlayer", 0, 3, 0)
        m.call("TestCueKind", 0, 14)  # Yoshi.
        m.call("TestCueProtection", 0, 0, 0, 12000)
        self.assertEqual(self.show(1, 27, 0), 0x4BE16470)
        m.call("TestCueProtection", 0, 0, 0, 0)
        m.call("TestCueDraw", 0)
        self.assertEqual(m.call("TestCueDrawColor", 0), 0)
        m.call("TestCueKind", 0, 1)
        m.call("TestCueProtection", 0, 0, 0, 12000)  # Other armor alone is not this explicit exception.
        self.assertEqual(self.show(2, 27, 1), 0)
        m.call("TestCueProtection", 0, 2, 0, 0)
        self.assertEqual(self.show(3, 341, 4), 0x4BE16470)  # Move-granted protection has no state whitelist.

    def test_version_two_migration_preserves_turnrun_and_30_reserved_bits(self):
        m = self.m
        old = bytearray(m.record())
        old[10] = 0xBF
        old[40:44] = b"\xFFABC"
        m.put(old)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
        self.assertEqual(m.record()[10], 0xFF)
        self.assertEqual(m.record()[40:44], b"\xFDABC")
        self.assertEqual(m.read(11, 6), 1)
        self.assertEqual(m.read(11, 7), 0)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        m.call("TestEditorInit")
        m.call("TestEditorInput", 0x200, 28)  # Protection at final grouped row.
        m.call("TestEditorAnimate", 28)
        m.write(16, 25, m.call("TestEditorCache", 28))
        self.assertEqual(m.record()[40:44], b"\xFFABC")
        fresh = Machine()
        fresh.put(m.record())
        self.assertEqual(fresh.call("Settings_Get", 11, 7), 1)


class LedgedashLogicTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.attempt = 0x80402000
        self.sample = 0x80403000
        self.m.call("LdshAttempt_Reset", self.attempt)

    def step(self, criterion, **changes):
        fields = ["on_ledge", "released", "airdodge", "grounded", "actionable", "galint", "attack_dash", "egg_pop", "dead", "ledge_option", "frozen", "target_ready"]
        values = dict.fromkeys(fields, 0)
        values["target_ready"] = 1
        values.update(changes)
        self.m.cpu.mem_write(self.sample, struct.pack(">12i", *(values[x] for x in fields)))
        return self.m.call("LdshAttempt_Step", self.attempt, self.sample, criterion)

    def begin(self, criterion):
        self.assertEqual(self.step(criterion, on_ledge=1), 0)
        self.assertEqual(self.step(criterion, released=1), 0)

    def test_approach_inputs_do_not_fail_or_start_attempt_timer(self):
        for _ in range(250):
            self.assertEqual(self.step(0, airdodge=1), 0)
        self.assertEqual(struct.unpack(">2i", self.m.cpu.mem_read(self.attempt, 8)), (0, 0))
        self.begin(0)
        self.assertEqual(self.step(0, grounded=1, actionable=1, galint=5), 1)

    def test_galint_and_waveland_are_distinct(self):
        self.begin(0)
        self.assertEqual(self.step(0, airdodge=1, grounded=1, galint=8), 0)
        self.assertEqual(self.step(0, grounded=1, actionable=1, galint=3), 1)
        self.assertEqual(self.step(0, dead=1), 1)  # Resolved once, even if later conditions change.
        self.m.call("LdshAttempt_Reset", self.attempt)
        self.begin(1)
        self.assertEqual(self.step(1, airdodge=1, grounded=1, galint=0), 1)
        self.m.call("LdshAttempt_Reset", self.attempt)
        self.begin(1)
        self.assertEqual(self.step(1, grounded=1, actionable=1, galint=9), 2)  # NIL is not a waveland.

    def test_protected_action_waits_and_fails_when_window_expires(self):
        self.begin(2)
        self.assertEqual(self.step(2, airdodge=1, grounded=1), 0)
        self.assertEqual(self.step(2, grounded=1, actionable=1, galint=4), 0)
        self.assertEqual(self.step(2, grounded=1, galint=3, attack_dash=1), 1)
        self.m.call("LdshAttempt_Reset", self.attempt)
        self.begin(2)
        self.assertEqual(self.step(2, grounded=1, actionable=1, galint=2), 0)
        self.assertEqual(self.step(2, grounded=1, galint=0, attack_dash=1), 2)

    def test_egg_pop_uses_collision_eligibility_and_unavailable_setup_is_excluded(self):
        self.begin(3)
        self.assertEqual(self.step(3, airdodge=1, grounded=1), 0)
        self.assertEqual(self.step(3, grounded=1, actionable=1, galint=4), 0)
        self.assertEqual(self.step(3, grounded=1, galint=3), 0)  # Damage without a pop is not success.
        self.assertEqual(self.step(3, egg_pop=1, galint=0, frozen=1), 1)  # Verified last-frame collision.
        self.m.call("LdshAttempt_Reset", self.attempt)
        self.assertEqual(self.step(3, on_ledge=1, target_ready=0), 0)
        self.assertEqual(self.step(3, released=1, target_ready=0), 3)

    def test_attempt_timeout_death_and_hitlag(self):
        self.begin(0)
        before = bytes(self.m.cpu.mem_read(self.attempt, 32))
        for _ in range(300): self.assertEqual(self.step(0, frozen=1), 0)
        self.assertEqual(bytes(self.m.cpu.mem_read(self.attempt, 32)), before)
        self.assertEqual(self.step(0, dead=1), 2)
        self.m.call("LdshAttempt_Reset", self.attempt)
        self.begin(0)
        for _ in range(179): self.assertEqual(self.step(0), 0)
        self.assertEqual(self.step(0), 2)

    def test_surface_selection_ground_platform_and_final_destination(self):
        surface, result = 0x80401000, 0x80401100
        def choose(values, platform, desired=20):
            self.m.cpu.mem_write(surface, struct.pack(">4f2i", *values))
            ok = self.m.call("TestLdshSurface", surface, desired * 100, 0, platform, result)
            return ok, struct.unpack(">2f", self.m.cpu.mem_read(result, 8))
        fd = (-85, 0, 85, 0, 0, 1)
        self.assertEqual(choose(fd, 1)[0], 0)  # Platform request cannot invent an airborne target.
        self.assertEqual(choose(fd, 0), (1, (20.0, 0.0)))  # Explicit Ground fallback.
        platform = (-30, 25, 30, 25, 1, 1)
        self.assertEqual(choose(platform, 1), (1, (20.0, 25.0)))
        self.assertEqual(choose(platform, 0)[0], 0)
        self.assertEqual(choose(platform, 1, 60), (1, (27.0, 25.0)))
        self.assertEqual(choose((-2, 0, 2, 0, 0, 1), 0)[0], 0)
        self.assertEqual(choose((-85, 0, 85, 0, 0, 0), 0)[0], 0)

    def test_attack_dash_classifier_keeps_grabs_and_adds_initial_dash(self):
        m = self.m
        self.assertEqual(m.call("Ldsh_IsAttackDash", 1, 20), 1)
        self.assertEqual(m.call("Ldsh_IsAttackDash", 1, 21), 0)  # Sustained run is distinct.
        self.assertEqual(m.call("Ldsh_IsAttackDash", 1, 14), 0)
        self.assertEqual(m.call("Ldsh_IsAttackDash", 2, 44), 1)

    def test_original_reset_checks_and_harder_criterion_opportunity(self):
        m=self.m
        # Original falling-start airdodge, actual failures and grounded cutoff.
        self.assertEqual(m.call("Ldsh_LegacyResetFailure",236,8,0,0,0,0),0)
        self.assertEqual(m.call("Ldsh_LegacyResetFailure",236,9,0,0,0,0),1)
        self.assertEqual(m.call("Ldsh_LegacyResetFailure",14,12,0,1,1,0),1)
        self.assertEqual(m.call("Ldsh_LegacyResetFailure",14,12,0,1,1,1),0)
        for state in [42,43,13]:
            self.assertEqual(m.call("Ldsh_LegacyResetFailure",state,20,0,1,1,0),0)
        self.assertEqual(m.call("Ldsh_LegacyResetFailure",0,0,1,0,0,1),1)

    def test_first_recovered_jump_still_counts_default_galint(self):
        self.begin(0)
        self.assertEqual(self.step(0, airdodge=1, grounded=1, galint=8),0)
        self.assertEqual(self.step(0, grounded=0, actionable=1, galint=4),1)

    def test_random_egg_distance_inclusive_bounds_and_reversed_limits(self):
        m=self.m
        for low,high in [(10,35),(35,10),(20,20)]:
            distances=[m.call("Ldsh_RandomDistance",low,high,roll) for roll in range(abs(high-low)+1)]
            self.assertEqual(distances,list(range(min(low,high),max(low,high)+1)))


if __name__ == "__main__":
    unittest.main()
