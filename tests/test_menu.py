"""PowerPC tests for canonical Lab menu views and shared controller rules."""
from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import ppc_const as reg


def build():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    source = ROOT / "tests/menu_probe.c"
    exports = re.findall(r"\b(TestMenu\w+)\(", source.read_text())
    subprocess.run([
        str(compiler) if compiler.exists() else "powerpc-eabi-gcc",
        "-O2", "-w", "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-ffunction-sections", "-fdata-sections",
        "-Wl,-Ttext=0x101000,-e,TestMenuInit,--gc-sections,--unresolved-symbols=ignore-all",
        *["-Wl,--undefined=" + name for name in exports], str(source), "-o", str(ROOT / "build/menu-test.elf")
    ], cwd=ROOT, check=True)


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x100000, 0x100000)
        self.cpu.mem_map(0x80000000, 0x500000)
        elf = (ROOT / "build/menu-test.elf").read_bytes()
        shoff = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, shoff + i * stride) for i in range(count)]
        self.symbols = {}
        for s in sections:
            _, kind, flags, address, offset, size, link, _, _, entry = s
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[offset:offset + size])
            if kind == 2:
                strings = sections[link]
                names = elf[strings[4]:strings[4] + strings[5]]
                for at in range(0, size, entry):
                    name, value, *_ = struct.unpack_from(">IIIBBH", elf, offset + at)
                    self.symbols[names[name:].split(b"\0", 1)[0].decode()] = value
        self.call("TestMenuInit")

    def call(self, name, *args):
        self.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x100000)
        for i, value in enumerate(args, 3):
            self.cpu.reg_write(getattr(reg, "UC_PPC_REG_" + str(i)), value & 0xFFFFFFFF)
        self.cpu.emu_start(self.symbols[name], 0x100000, count=100000)
        value = self.cpu.reg_read(reg.UC_PPC_REG_3)
        return value if value < 0x80000000 else value - 0x100000000

    def name(self, menu):
        address = self.call("TestMenuName", menu)
        return bytes(self.cpu.mem_read(address, 80)).split(b"\0", 1)[0].decode()

    def graph(self):
        origins, names, pages = {}, {}, set()
        def visit(menu, depth):
            name = self.name(menu)
            self.assert_depth(depth)
            for page in range(max(1, self.call("TestMenuPages", menu))):
                self.call("TestMenuPage", menu, page)
                if (menu, page) in pages:
                    continue
                pages.add((menu, page))
                count = self.call("TestMenuCount", menu)
                if count > 9:
                    raise AssertionError((name, page, count))
                for row in range(count):
                    option = self.call("TestMenuOption", menu, row)
                    origin = self.call("TestMenuOrigin", option)
                    if origin:
                        origins.setdefault(origin, []).append(name)
                    names.setdefault(name, []).append(option)
                    target = self.call("TestMenuTarget", option)
                    if target:
                        visit(target, depth + 1)
            self.call("TestMenuPage", menu, 0)
        for tab in range(self.call("TestMenuTabCount")):
            visit(self.call("TestMenuTab", tab), 1)
        return origins, names, pages

    @staticmethod
    def assert_depth(depth):
        if depth > 3:
            raise AssertionError("Menu exceeds three entries")


class MenuTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def test_cursor_boundaries_and_one_step(self):
        for index, count, selector, direction, expected in [
            (-1, 9, 1, 1, 0), (-1, 9, 1, -1, 8), (0, 9, 1, -1, -1),
            (8, 9, 1, 1, -1), (0, 2, 0, -1, 1), (1, 2, 0, 1, 0), (0, 0, 0, 1, 0)
        ]:
            self.assertEqual(self.m.call("TestMenuMove", index, count, selector, direction), expected)
        for value, minimum, count, direction, expected in [
            (-127, -127, 255, -1, -127), (-127, -127, 255, 1, -126),
            (127, -127, 255, 1, 127), (0, 0, 2, -1, 0), (1, 0, 2, 1, 1), (1, 1, 3600, 1, 2)
        ]:
            self.assertEqual(self.m.call("TestMenuStep", value, minimum, count, direction), expected)

    def test_large_picker_and_viewport(self):
        for args, expected in [((0, 8, 20, 9), 0), ((0, 9, 20, 9), 1), ((11, 2, 20, 9), 2),
                               ((0, 3599, 3600, 9), 3591), ((50, 0, 2, 9), 0)]:
            self.assertEqual(self.m.call("TestMenuScroll", *args), expected)

    def test_readable_preview_spacing_and_panel_bounds(self):
        step = self.m.call("TestMenuPreviewStep")
        bottom = self.m.call("TestMenuPreviewBottom")
        # 32-unit glyph at scale .84 has height 26.88; allow at least 4 units gap.
        self.assertGreaterEqual(step, 31)
        self.assertGreaterEqual(self.m.call("TestMenuDescriptionStep"), 29)
        for description_lines in range(9):
            first = self.m.call("TestMenuPreviewStart", description_lines)
            capacity = self.m.call("TestMenuPreviewCapacity", first)
            self.assertGreaterEqual(capacity, 3)
            self.assertLessEqual(first + (capacity - 1) * step + 32, bottom)
            slots = capacity - 2
            pages = (20 + slots - 1) // slots
            self.assertEqual(sum(min(slots, 20 - page * slots) for page in range(pages)), 20)

    def test_complete_bounded_views_and_boost_grab(self):
        origins, names, pages = self.m.graph()
        self.assertEqual(self.m.call("TestMenuTabCount"), 8)
        for array, count in [(1, 18), (2, 24), (3, 18), (4, 16), (6, 6), (7, 17), (8, 17), (9, 10), (10, 10), (11, 13)]:
            self.assertTrue(set(range(array * 100, array * 100 + count)).issubset(origins), array)
        self.assertEqual(set(range(500, 519)), {i for i in origins if 500 <= i < 519})
        self.assertTrue(all(len(origins[i]) == 1 for i in range(500, 519)))
        self.assertEqual(origins[509], ["Combat & Defense OSDs"])
        self.assertEqual(len(names["Movement & Landing OSDs"]), 6)
        self.assertEqual(len(names["Combat & Defense OSDs"]), 8)
        self.assertGreater(len(pages), 40)

    def test_views_follow_canonical_values(self):
        session = self.m.call("TestMenuTab", 0)
        option = self.m.call("TestMenuOption", session, 2)
        self.assertEqual(self.m.call("TestMenuOrigin", option), 101)
        self.m.call("TestMenuWriteSource", 1, 1, 77)
        self.assertEqual(self.m.call("TestMenuValue", option), 77)

    def test_optimized_lab_dat_views_after_native_relocation(self):
        """Validate production pointers, not just the ELF compiler layout."""
        dat = (ROOT / "build/lab.dat").read_bytes()
        _, size, relocations, roots, refs = struct.unpack_from(">5I", dat)
        nodes = 32 + size + relocations * 4
        strings = nodes + (roots + refs) * 8
        header = None
        for i in range(roots):
            offset, name = struct.unpack_from(">II", dat, nodes + i * 8)
            if dat[strings + name:].split(b"\0", 1)[0] == b"evFunction":
                header = struct.unpack_from(">6I", dat, 32 + offset)
        self.assertIsNotNone(header)
        code, table, count, exports, export_count, payload_size = header
        base = 0x80700000
        payload = bytearray(dat[32 + code:32 + code + payload_size])
        for i in range(count):
            word, target = struct.unpack_from(">II", dat, 32 + table + i * 8)
            kind, at = word >> 24, word & 0xFFFFFF
            if target & 0xF0000000 != 0x80000000:
                target = (target + base) & 0xFFFFFFFF
            if kind == 1:
                struct.pack_into(">I", payload, at, target)
            elif kind == 4:
                struct.pack_into(">H", payload, at, target & 0xFFFF)
            elif kind == 6:
                struct.pack_into(">H", payload, at, ((target + 0x8000) >> 16) & 0xFFFF)
            elif kind == 10:
                old = struct.unpack_from(">I", payload, at)[0]
                struct.pack_into(">I", payload, at, old | ((target - base - at) & 0x03FFFFFC))
            elif kind == 26:
                struct.pack_into(">I", payload, at, (target - base - at) & 0xFFFFFFFF)
            else:
                self.fail("Unexpected relocation " + str(kind))
        self.m.cpu.mem_map(0x80500000, 0x1300000)
        self.m.cpu.mem_write(base, bytes(payload))
        public = dict(struct.unpack_from(">II", dat, 32 + exports + i * 8) for i in range(export_count))
        # Event_Init begins by assigning the canonical navigation targets/pages.
        # Stop at its first game dependency, before any actual fighter is read.
        reached = []
        def stop_at_fighter(cpu, address, size, user):
            reached.append(address)
            cpu.emu_stop()
        hook = self.m.cpu.hook_add(UC_HOOK_CODE, stop_at_fighter, begin=0x80034110, end=0x80034110)
        self.m.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.m.cpu.emu_start(base + public[0], 0x100000, count=100000)
        self.m.cpu.hook_del(hook)
        self.assertEqual(reached, [0x80034110])
        def word(address):
            self.assertTrue(base <= address <= base + payload_size - 4, hex(address))
            return struct.unpack(">I", self.m.cpu.mem_read(address, 4))[0]
        def byte(address):
            return self.m.cpu.mem_read(address, 1)[0]
        def text(address):
            self.assertTrue(base <= address < base + payload_size, hex(address))
            return bytes(self.m.cpu.mem_read(address, 100)).split(b"\0", 1)[0].decode()
        root = word(base + public[3])
        self.assertEqual(byte(root + 36), 8)
        tabs = word(root + 32)
        observed, visited, option_pointers = {}, set(), {}
        def walk(menu, depth):
            self.assertLessEqual(depth, 3)
            if menu in visited:
                return
            visited.add(menu)
            name = text(word(menu))
            purpose = word(menu + 40)
            if purpose:
                text(purpose)
            page_count = byte(menu + 28)
            for page in range(max(1, page_count)):
                if page_count:
                    descriptor = word(menu + 24) + page * 12
                    count, view, options = byte(descriptor + 4), word(descriptor + 8), 0
                else:
                    count, view, options = byte(menu + 4), word(menu + 20), word(menu + 8)
                self.assertLessEqual(count, 9, name)
                for i in range(count):
                    option = word(view + i * 8) if view else options + i * 44
                    label = word(option + 12)
                    if label:
                        option_label = text(label)
                        observed.setdefault(option_label, []).append(name)
                        option_pointers[option_label] = option
                    for desc in range(4):
                        description = word(option + 16 + desc * 4)
                        if description:
                            text(description)
                    if byte(option) == 0:
                        target = word(option + 32)
                        if target:
                            walk(target, depth + 1)
        for i in range(8):
            text(word(tabs + i * 8))
            walk(word(tabs + i * 8 + 4), 1)
        self.assertEqual(observed["Boost Grab"], ["Combat & Defense OSDs"])
        self.assertEqual(observed["Frame Advance"], ["Session"])
        self.assertIn("Recording", observed["Save Positions"])
        # The opt-in root callback lives in the relocated Lab, not shared-code
        # exports. Simulate a disabled recording mode before the state exists.
        reason_callback = word(root + 44)
        self.assertTrue(base <= reason_callback < base + payload_size)
        mode = option_pointers["HMN Mode"]
        self.m.cpu.mem_write(mode + 1, b"\1")
        self.m.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.m.cpu.reg_write(reg.UC_PPC_REG_3, mode)
        self.m.cpu.reg_write(reg.UC_PPC_REG_LR, 0x100000)
        self.m.cpu.emu_start(reason_callback, 0x100000, count=100000)
        self.assertEqual(text(self.m.cpu.reg_read(reg.UC_PPC_REG_3)), "Save Positions first to unlock recording controls.")


if __name__ == "__main__":
    build()
    unittest.main(verbosity=2)
