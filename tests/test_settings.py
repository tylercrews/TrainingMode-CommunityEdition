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
        for flag in range(6):
            m.write(11, flag, 1)
        self.assertEqual(m.record()[10], 0x7F)
        m.write(4, 0, 4)
        m.write(5, 0, 5)
        self.assertEqual(m.record()[7], 0x54)
        m.write(15, IDS[2], 1)
        self.assertEqual(m.read(14, IDS[2]), 2)  # Boolean writer preserves an enabled Red choice.
        for field, index, value in [(4, 0, 5), (5, 0, 6), (11, 6, 1), (12, 18, 1), (14, 64, 1), (15, 32, 1)]:
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
        self.assertEqual(m.record()[10], 0x40)
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
        future[10] = 0xBF
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
        for mode, lifetime in enumerate([65, 35, 21, 5, 1, 130]):
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, 0), 200)
            for age in range(1, lifetime):
                alpha = self.m.call("TMTrail_Alpha", mode, age)
                self.assertGreater(alpha, 0)
                self.assertLessEqual(alpha, 72)
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, lifetime), 0)
        self.assertEqual(self.m.call("TMTrail_Alpha", 6, 0xFFFFFFFF), 72)
        self.assertEqual(self.m.call("TMTrail_Alpha", 99, 0), 0)

    def test_union_global_priority_and_player_palette(self):
        self.assertEqual(self.m.call("TMTrail_Effective", 1, 1, 1, 5), 2)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 1, 1, 5), 4)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 1, 3), 3)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 0, 0), 7)
        self.assertEqual(self.m.call("TMTrail_Effective", 0, 0, 1, 99), 7)
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


if __name__ == "__main__":
    unittest.main()
