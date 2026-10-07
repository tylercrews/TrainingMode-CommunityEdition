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
from unicorn import Uc, UcError, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn import ppc_const as reg

RECORD = 0x8045A6C0 + 0x1F24
STOP = 0x100000
IDS = [0, 1, 3, 5, 8, 9, 10, 12, 13, 14, 16, 18, 19, 20, 21, 22, 24, 26, 28]


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def styled_ascii(m, line):
    raw=bytes(m.call("TestStyleStringChar",line,i) for i in range(112)).split(b"\0",1)[0]
    result=[]; color=None; i=0
    while i < len(raw):
        if raw[i] == 0x1B:
            color=int(raw[i+1:i+9],16);i+=9
        else: result.append((chr(raw[i]),color));i+=1
    return "".join(c for c,_ in result),result,raw


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x100000, 0x100000)
        self.cpu.mem_map(0x80000000, 0x500000)
        # Verified against the local native DOL, function 0x800C0658.
        self.cpu.mem_write(0x800C0658, bytes.fromhex("800304302c0000004182000c386304084e800020386304884e800020"))
        dol = (ROOT / "build/Start.dol").read_bytes()
        for address, size in [(0x8040C680,574),(0x8040C8C0,574),(0x8040CB00,640),
                              (0x803A67EC,0x3AC),(0x803A6B98,0x1F24),
                              (0x8040C568,0xAC),(0x804DE000,0xC00)]:
            for section in range(18):
                offset, base, length = [struct.unpack_from(">I",dol,at+section*4)[0] for at in (0,0x48,0x90)]
                if base <= address and address + size <= base + length:
                    self.cpu.mem_write(address,dol[offset+address-base:offset+address-base+size])
                    break
            else: raise AssertionError("Native font tables missing from input DOL")
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

    def install_hook(self, address, allocation=0x80480000):
        data = (ROOT / "build/settings-codes-test.gct").read_bytes()
        needle = struct.pack(">I", 0xC2000000 | (address & 0x01FFFFFF))
        offset = data.find(needle)
        if offset < 0: raise AssertionError(f"Native C2 hook missing at {address:x}")
        lines = struct.unpack_from(">I",data,offset+4)[0]
        body = bytearray(data[offset+8:offset+8+lines*8])
        assert len(body) == lines*8
        struct.pack_into(">I",body,len(body)-4,0x48000000|((address+4-allocation-len(body)+4)&0x03FFFFFC))
        self.cpu.mem_write(allocation,bytes(body))
        self.cpu.mem_write(address,struct.pack(">I",0x48000000|((allocation-address)&0x03FFFFFC)))
        return allocation

    def native_text(self, value):
        self.install_hook(0x803A684C)
        source,output=0x80403000,0x80403200
        self.cpu.mem_write(source,value+b"\0")
        self.cpu.mem_write(output-8,b"\xA5"*8+b"\0"*160+b"\xA5"*8)
        self.symbols["NativeTextConvert"] = 0x803A67EC
        size=self.call("NativeTextConvert",output,source)
        assert size <= 160
        assert bytes(self.cpu.mem_read(output-8,8)) == b"\xA5"*8
        assert bytes(self.cpu.mem_read(output+160,8)) == b"\xA5"*8
        return bytes(self.cpu.mem_read(output,size))

    def native_width(self, encoded):
        """Execute the native parser at the PC shown in the user's warning."""
        stream,text,output,sis=0x80403400,0x80403600,0x80403800,0x80403A00
        self.cpu.mem_write(stream,encoded+b"\0")
        self.cpu.mem_write(text,bytes(0xA4))
        self.cpu.mem_write(text+0x68,struct.pack(">IHH",0x80403C00,0,64))
        self.cpu.mem_write(0x80403C00,bytes(64))
        self.cpu.mem_write(text+0x78,struct.pack(">3f",0,1,1))
        self.cpu.mem_write(text+0x9D,b"\x01")  # Native glyph kerning enabled.
        self.cpu.mem_write(0x804D1124,struct.pack(">I",sis))
        # Built-in 0x20xx glyphs use the menu table; ordinary 0x40xx ASCII
        # uses bounded custom-SIS kerning. The old stray 0xFF glyph indexes
        # beyond that mapped table, reproducing the pointer warning.
        self.cpu.mem_write(sis,struct.pack(">2I",0,0x001FF800))
        self.cpu.mem_write(0x001FF800,b"\x04\x08"*256)
        self.symbols["NativeWidth"] = 0x803A8134
        saved_r2=self.cpu.reg_read(gpr(2))
        self.cpu.reg_write(gpr(2),0x804DF9E0)
        self.cpu.reg_write(gpr(1),0x80400000)
        try: self.call("NativeWidth",stream,text,output,output+4)
        finally: self.cpu.reg_write(gpr(2),saved_r2)
        return struct.unpack(">2f",self.cpu.mem_read(output,8))

    def native_subtexts(self, rows):
        """Use the same native locator used by SetText/Position/Scale/Color."""
        stream=0x80404400
        chunks=[b"\x07"+struct.pack(">2h",0,i*30)+b"\x0c\xff\xff\xff\x0e\x01\0\x01\0"+
                row+b"\x0f\x0d" for i,row in enumerate(rows)]
        self.cpu.mem_write(stream,b"".join(chunks)+b"\0")
        self.symbols["NativeSubtext"] = 0x803A6FEC
        expected=stream
        for i,chunk in enumerate(chunks):
            found=self.call("NativeSubtext",stream,i,0)
            if found != expected: raise AssertionError(f"Row {i} corrupted: {found:x} != {expected:x}")
            expected+=len(chunk)
        return chunks

    def native_rewrite_rows(self, sources, updates, patch_iterator=True):
        """Use real SetText/Position/Scale/Color on one complete text buffer."""
        if patch_iterator: self.install_hook(0x803A7068,0x80481000)
        rows=[self.native_text(source) for source in sources]
        self.native_subtexts(rows)
        stream,text,pool=0x80410000,0x80404000,0x80404300
        chunks=[b"\x07"+struct.pack(">2h",0,i*30)+b"\x0c\xff\xff\xff\x0e\x01\0\x01\0"+
                row+b"\x0f\x0d" for i,row in enumerate(rows)]
        initial=b"".join(chunks)
        self.cpu.mem_write(stream-8,b"\xA5"*8+b"\0"*4096+b"\xA5"*8)
        self.cpu.mem_write(stream,initial+b"\0")
        self.cpu.mem_write(text,bytes(0xA4))
        self.cpu.mem_write(text+0x5C,struct.pack(">3I",stream,stream+len(initial)-2,pool))
        self.cpu.mem_write(pool,struct.pack(">4I",stream+len(initial),stream,4096,len(rows)))
        address=self.symbols["TestTextCopyFormat"]
        self.cpu.mem_write(0x80323DC8,struct.pack(">4I",0x3D800000|(address>>16),
            0x618C0000|(address&65535),0x7D8903A6,0x4E800420))
        self.symbols.update(NativeSetText=0x803A70A0,NativePosition=0x803A746C,
                            NativeScale=0x803A7548,NativeColor=0x803A74F0,NativeSubtext=0x803A6FEC)
        saved_r2=self.cpu.reg_read(gpr(2))
        try:
            self.cpu.reg_write(gpr(2),0x804DF9E0)
            for index,source in updates:
                expected=self.native_text(source);rows[index]=expected
                self.cpu.mem_write(0x80403000,source+b"\0")
                self.call("NativeSetText",text,index,0x80403000)
                for function,x,y in [("NativePosition",0.0,index*30.0),("NativeScale",1.0,1.0)]:
                    self.cpu.reg_write(reg.UC_PPC_REG_FPR1,struct.unpack(">Q",struct.pack(">d",x))[0])
                    self.cpu.reg_write(reg.UC_PPC_REG_FPR2,struct.unpack(">Q",struct.pack(">d",y))[0])
                    self.call(function,text,index)
                self.cpu.mem_write(0x80403E00,b"\xff\xff\xff\xff")
                self.call("NativeColor",text,0,0x80403E00)
                current=struct.unpack(">I",self.cpu.mem_read(pool,4))[0]
                self.assert_stream_rows(stream,current,rows)
                assert bytes(self.cpu.mem_read(stream-8,8)) == b"\xA5"*8
                assert bytes(self.cpu.mem_read(stream+4096,8)) == b"\xA5"*8
        finally: self.cpu.reg_write(gpr(2),saved_r2)
        return rows

    def assert_stream_rows(self, stream, current, rows):
        for index,expected in enumerate(rows):
            group=self.call("NativeSubtext",stream,index,0)
            if not group: raise AssertionError(f"Missing native row {index}")
            body=group+14
            actual=bytes(self.cpu.mem_read(body,len(expected)+2))
            if actual != expected+b"\x0f\x0d":
                raise AssertionError(f"Row {index} retained/corrupted text: {actual.hex()}")
            self.native_width(expected)
        last=self.call("NativeSubtext",stream,len(rows)-1,0)
        if last+14+len(rows[-1])+2 != current:
            raise AssertionError("Native buffer end/row boundaries changed incorrectly")

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
            if index in [27, 28, 38]:
                self.m.symbols[{27:"DAT_Get",28:"DAT_Set",38:"DAT_Commit"}[index]] = self.base + offset
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
        m.call("DAT_Set", 17, 18, 1)  # Separator is not a saved preference.
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

    def test_event_preferences_through_real_dat_relocation_and_reload(self):
        m = self.m
        m.init()
        m.call("DAT_Set", 11, 8, 1)
        m.call("DAT_Set", 18, 4, 0)  # Hints Off.
        m.call("DAT_Set", 18, 0, 4)
        m.call("DAT_Set", 19, 0, 199)
        m.call("DAT_Set", 19, 4, 1)  # Infinite mode On.
        saved = m.record()
        fresh = Machine()
        fresh.put(saved)
        for _ in range(10):
            self.assertEqual(m.call("DAT_Get", 18, 4), 0)
            self.assertEqual(m.call("DAT_Get", 19, 4), 1)
            self.assertEqual(fresh.call("Settings_Get", 18, 0), 4)
            self.assertEqual(fresh.call("Settings_Get", 19, 0), 199)
            self.assertEqual(fresh.call("Settings_Get", 19, 4), 1)
            self.assertEqual(fresh.call("Settings_Get", 11, 8), 1)
        self.assertEqual(fresh.record(), saved)
        self.assertEqual(fresh.call("TestDirty"), 0)

    def test_layout_and_runtime_page_use_relocated_accessors_without_touching_preferences(self):
        m = self.m; m.init()
        m.call("DAT_Set",19,4,1); m.call("DAT_Set",18,4,0)
        before = m.record()
        for choice in range(6):
            m.call("DAT_Set",22,0,choice)
            self.assertEqual(m.call("DAT_Get",22,0),choice)
            self.assertEqual(m.call("DAT_Get",19,4),1)
            self.assertEqual(m.call("DAT_Get",18,4),0)
        saved=m.record()
        m.call("DAT_Set",23,0,4)
        self.assertEqual(m.call("DAT_Get",23,0),4)
        self.assertEqual(m.record(),saved)
        self.assertEqual(m.record()[40:43],before[40:43])


class NativeCardSaveTests(unittest.TestCase):
    STATE=0x80433318
    WORK=0x80432A68
    ICONS=0x80407000

    def setUp(self):
        self.m=Machine();self.m.init()
        self.m.cpu.mem_write(self.STATE,bytes(0x68))
        self.m.cpu.mem_write(self.WORK,bytes(8))
        dol=(ROOT/'build/Start.dol').read_bytes()
        start,size=0x8001C600,0x800
        for section in range(18):
            offset,base,length=[struct.unpack_from('>I',dol,at+section*4)[0] for at in (0,0x48,0x90)]
            if base<=start and start+size<=base+length:
                self.m.cpu.mem_write(start,dol[offset+start-base:offset+start-base+size]);break
        else:self.fail('Native card routines missing')
        self.m.symbols['NativeSave']=0x8001CC84
        self.m.symbols['NativeSaveWait']=0x8001CDB4
        self.leaf(0x80304470,0);self.leaf(0x80164ABC,1)
        self.leaf(0x8001C658,0x80408000) # Date/banner string, not part of the null-table failure.
        self.leaf(0x8001BE30,11) # Successful asynchronous request, no real card is written.
        self.leaf(0x8001B6F8,0)
        self.m.cpu.mem_write(self.ICONS,struct.pack('>4I',*[0x80408000]*4))

    def leaf(self,address,value):
        self.m.cpu.mem_write(address,struct.pack('>3I',0x3C600000|(value>>16),0x60630000|(value&65535),0x4E800020))

    def ready(self,enable=1,icons=ICONS,work=0x80409000,buffer=0x8040A000,card_state=0):
        m=self.m
        m.cpu.mem_write(self.STATE+0x18,struct.pack('>I',enable))
        m.cpu.mem_write(self.STATE+0x5C,struct.pack('>I',icons))
        m.cpu.mem_write(self.STATE+8,struct.pack('>I',card_state))
        m.cpu.mem_write(self.WORK,struct.pack('>2I',work,buffer))

    def test_unpatched_save_reproduces_the_reported_pc_and_null_read(self):
        m=self.m;m.cpu.mem_write(self.STATE+0xC,struct.pack('>I',1))
        with self.assertRaises(UcError):m.call('NativeSave')
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_PC),0x8001C868)
        self.assertEqual(m.cpu.reg_read(gpr(3))+m.cpu.reg_read(gpr(0)),4)

    def test_native_poll_and_wait_defer_each_uninitialized_resource_without_losing_dirty(self):
        m=self.m;m.install_hook(0x8001CC84,0x80481000);m.install_hook(0x8001CDB4,0x80482000)
        for missing in ['enable','icons','work','buffer']:
            self.ready(**{missing:0})
            m.cpu.mem_write(self.STATE+0xC,struct.pack('>I',1))
            m.call('NativeSave');m.call('NativeSaveWait')
            self.assertEqual(m.call('TestDirty'),1)
            self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.STATE+0x10,4))[0],0)

    def test_both_events_queue_through_match_unload_then_commit_to_ready_native_pipeline(self):
        m=self.m;m.install_hook(0x8001CC84,0x80481000)
        m.call('Settings_Set',18,0,2);m.call('Settings_Set',19,4,1)
        saved=m.record()
        self.assertEqual(m.call('TestDirty'),0)
        m.call('NativeSave');m.call('Settings_CommitPending')
        self.assertEqual(m.call('TestDirty'),0)
        m.cpu.mem_write(self.STATE,bytes(0x68)) # Actual archive teardown resets native dirty.
        self.ready(card_state=3)
        m.call('Settings_CommitPending');self.assertEqual(m.call('TestDirty'),0)
        self.ready();m.call('Settings_CommitPending')
        self.assertEqual(m.call('TestDirty'),1)
        m.call('NativeSave')
        self.assertEqual(m.call('TestDirty'),0)
        self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.STATE+0x10,4))[0],1)
        self.assertEqual(m.record(),saved)
        self.assertEqual(m.read(18,0),2);self.assertEqual(m.read(19,4),1)
        m.call('Settings_CommitPending') # Acknowledge the accepted request, not merely the dirty mark.
        m.call('NativeSave');self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.STATE+0x10,4))[0],0)
        m.call('Settings_CommitPending');self.assertEqual(m.call('TestDirty'),0)

    def test_safe_commit_respects_identity_and_real_ready_state(self):
        m=self.m;m.call('Settings_Set',18,4,0)
        for missing in ['enable','icons','work','buffer']:
            self.ready(**{missing:0});m.call('Settings_CommitPending')
            self.assertEqual(m.call('TestDirty'),0)
        self.ready();m.cpu.mem_write(0x80000000,b'GTME01')
        m.call('Settings_CommitPending');self.assertEqual(m.call('TestDirty'),0)
        m.cpu.mem_write(0x80000000,b'TYRE01');m.call('Settings_CommitPending')
        self.assertEqual(m.call('TestDirty'),1)

    def test_commit_retry_survives_leaving_menu_before_native_request_acceptance(self):
        m=self.m;m.call('Settings_Set',19,0,15)
        self.ready();m.call('Settings_CommitPending');self.assertEqual(m.call('TestDirty'),1)

    def test_unrelated_inflight_request_cannot_acknowledge_unqueued_match_edits(self):
        m=self.m;m.call('Settings_Set',19,4,1)
        self.ready();m.cpu.mem_write(self.STATE+0x10,struct.pack('>I',1))
        m.call('Settings_CommitPending')
        self.assertEqual(m.call('TestDirty'),1)
        m.cpu.mem_write(self.STATE,bytes(0x68)) # Leave before the next native poll.
        self.ready();m.call('Settings_CommitPending');self.assertEqual(m.call('TestDirty'),1)


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
        for field, index, value in [(4, 0, 5), (5, 0, 6), (11, 9, 1), (12, 18, 1), (14, 64, 1), (15, 32, 1)]:
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
        self.assertEqual(m.record()[40:44], b"K@EP")  # Invalid saved start=5 is repaired; free bits survive.

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
        self.assertEqual(m.call("TestDirty"), 0)  # Match edit is queued, not sent to an unloaded card service.


class EventPreferenceTests(unittest.TestCase):
    LEDGE = 18
    EGGS = 19
    RESET = 20
    defaults = {LEDGE: [0, 1, 0, 1, 1], EGGS: [12, 0, 1, 0, 0]}
    counts = {LEDGE: [5, 5, 4, 4, 2], EGGS: [200, 3, 2, 2, 2]}

    def setUp(self):
        self.m = Machine()
        self.m.init()

    def tearDown(self):
        self.assertEqual(bytes(self.m.cpu.mem_read(RECORD - 8, 8)), b"\xA5" * 8)
        self.assertEqual(bytes(self.m.cpu.mem_read(RECORD + 44, 8)), b"\xA5" * 8)

    def values(self, m):
        return {field: [m.read(field, i) for i in range(len(values))] for field, values in self.defaults.items()}

    def test_old_format_three_uses_read_only_defaults_until_first_edit(self):
        m = self.m
        old = bytearray(m.record())
        old[40:44] = b"\xf0\xff\xff\x9f"  # Initialization bit clear; Recent layout; arbitrary preference payload.
        m.put(old)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        self.assertEqual(self.values(m), self.defaults)
        self.assertEqual(m.record(), old)
        self.assertEqual(m.call("Settings_Get", self.EGGS, 4), 0)
        self.assertEqual(m.call("TestDirty"), 0)
        m.call("Settings_Set", self.LEDGE, 4, 0)
        self.assertEqual(m.call("TestDirty"), 0)  # Native dirty is deferred until card resources are ready.
        self.assertEqual(m.record()[:40], old[:40])
        self.assertEqual(m.record()[40] & 0x80, 0x80)
        self.assertEqual(m.record()[43] & 0x80, 0x80)
        self.assertEqual(self.values(m), {self.LEDGE: [0, 1, 0, 1, 0], self.EGGS: self.defaults[self.EGGS]})

    def test_every_supported_value_round_trips_without_changing_other_choices(self):
        m = self.m
        reserve = bytearray(m.record())
        reserve[40] |= 0x87
        reserve[43] |= 0x80
        m.put(reserve)
        for field, counts in self.counts.items():
            for index, count in enumerate(counts):
                for value in range(count):
                    before = self.values(m)
                    m.write(field, index, value)
                    before[field][index] = value
                    self.assertEqual(self.values(m), before)
                    self.assertEqual(m.record()[:40], reserve[:40])
                    self.assertEqual(m.record()[40] & 0x87, 0x87)
                    self.assertEqual(m.record()[43] & 0x80, 0x80)
                    saved = m.record()
                    self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
                    self.assertEqual(m.record(), saved)

    def test_exact_packed_budget_and_free_bits(self):
        m = self.m
        r = bytearray(m.record()); r[40] = 0x87; r[43] = 0x80; m.put(r)
        for field, values in [(self.LEDGE, [4, 4, 3, 3, 0]), (self.EGGS, [199, 2, 0, 1, 1])]:
            for index, value in enumerate(values): m.write(field, index, value)
        self.assertEqual(m.record()[40:44], b"\xbf\xe4\xc7\x9a")
        self.assertEqual(m.record()[10] >> 6, 3)
        self.assertEqual(len(m.record()), 44)

    def test_reject_invalid_writes_before_initializing_the_record(self):
        m = self.m
        before = m.record()
        for field, counts in self.counts.items():
            for index, count in enumerate(counts):
                for value in (count, 0xFFFFFFFF):
                    self.assertEqual(m.write(field, index, value), 0)
                    self.assertEqual(m.record(), before)
            self.assertEqual(m.write(field, len(counts), 0), 0)
            self.assertEqual(m.read(field, len(counts)), 0)
        for index, value in [(2, 1), (0, 0), (1, 2)]:
            self.assertEqual(m.write(self.RESET, index, value), 0)
        self.assertEqual(m.record(), before)

    def test_corrupt_initialized_values_repair_individually_and_idempotently(self):
        m = self.m
        r = bytearray(m.record()); r[40:44] = b"\xff\xff\xff\xff"; m.put(r)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 2)
        self.assertEqual(self.values(m), {self.LEDGE: [0, 1, 3, 3, 1], self.EGGS: [12, 0, 1, 1, 1]})
        self.assertEqual(m.record()[40:44], b"\xff\xc8\x0c\x9c")
        self.assertEqual(m.record()[:40], r[:40])
        saved = m.record()
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        self.assertEqual(m.record(), saved)

    def test_event_resets_preserve_sibling_preferences_flags_and_free_bits(self):
        m = self.m
        r = bytearray(m.record()); r[40] = 0x87; r[43] = 0x80; m.put(r)
        for field, counts in self.counts.items():
            for index, count in enumerate(counts): m.write(field, index, count - 1)
        eggs = self.values(m)[self.EGGS]
        m.write(self.RESET, 0, 1)
        self.assertEqual(self.values(m), {self.LEDGE: self.defaults[self.LEDGE], self.EGGS: eggs})
        m.write(self.LEDGE, 4, 0)
        ledge = self.values(m)[self.LEDGE]
        m.write(self.RESET, 1, 1)
        self.assertEqual(self.values(m), {self.LEDGE: ledge, self.EGGS: self.defaults[self.EGGS]})
        self.assertEqual(m.record()[40] & 0x87, 0x87)
        self.assertEqual(m.record()[43] & 0x80, 0x80)
        self.assertEqual(m.record()[:40], r[:40])

    def test_infinite_mode_and_hints_are_independent_and_survive_fresh_service(self):
        m = self.m
        m.call("Settings_Set", self.LEDGE, 4, 0)
        m.call("Settings_Set", self.EGGS, 4, 1)
        saved = m.record()
        fresh = Machine(); fresh.put(saved)
        self.assertEqual(fresh.call("Settings_Get", self.LEDGE, 4), 0)
        self.assertEqual(fresh.call("Settings_Get", self.EGGS, 4), 1)
        self.assertEqual(fresh.call("TestDirty"), 0)
        self.assertEqual(fresh.record(), saved)
        fresh.call("Settings_Set", self.LEDGE, 4, 1)
        self.assertEqual(fresh.call("Settings_Get", self.EGGS, 4), 1)

    def test_foreign_and_unsupported_records_only_use_private_event_preferences(self):
        for foreign in (False, True):
            m = Machine(); m.init()
            m.write(self.EGGS, 4, 1)
            if foreign: m.cpu.mem_write(0x80000000, b"GTME01")
            else:
                r = bytearray(m.record()); r[10] &= 0x3F; m.put(r)
            saved = m.record()
            self.assertEqual(m.call("Settings_Get", self.EGGS, 4), 0)
            m.call("Settings_Set", self.LEDGE, 4, 0)
            m.call("Settings_Set", self.EGGS, 4, 1)
            self.assertEqual(m.call("Settings_Get", self.EGGS, 4), 1)
            self.assertEqual(m.call("Settings_Get", self.LEDGE, 4), 0)
            self.assertEqual(m.record(), saved)
            self.assertEqual(m.call("TestDirty"), 0)


class OSDLayoutTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine(); self.m.init()

    def footer(self, object):
        return bytes(self.m.call("TestLayoutFooterChar", object, i) for i in range(120)).split(b"\0", 1)[0]

    def start(self, mode=1, ids=(0,1,8,16,20)):
        m = self.m
        for id in ids: m.write(14, id, 6)
        m.call("Settings_Set", 21, 0, mode)
        m.call("TestCueInit"); m.call("TestCueState", 0, 14, 1, 40, 100)
        m.call("TestLayoutInit"); m.call("TestLayoutTick", 0)

    def test_layout_bits_composite_choices_and_preferences_survive_reload(self):
        m = self.m
        r = bytearray(m.record()); r[40] = 0x87; r[43] = 0x80; m.put(r)
        m.write(18, 4, 0); m.write(19, 4, 1)
        before = m.record()
        for choice in range(6):
            m.call("Settings_Set", 22, 0, choice)
            self.assertEqual(m.call("Settings_Get", 22, 0), choice)
            self.assertLess(m.read(1), 4)
            self.assertEqual(m.read(18, 4), 0); self.assertEqual(m.read(19, 4), 1)
            self.assertEqual(m.record()[40:], bytes([before[40], before[41], before[42],
                (before[43] & ~0x60) | (max(0,choice-3) << 5)]))
            fresh = Machine(); fresh.put(m.record())
            self.assertEqual(fresh.call("Settings_Get", 22, 0), choice)
        m.call("Settings_Set", 21, 0, 2)
        self.assertEqual(m.call("Settings_Get", 22, 0), 5)
        m.write(20, 1, 1)
        self.assertEqual(m.read(21), 2)  # Egg reset cannot reset global layout.
        invalid = bytearray(m.record()); invalid[43] |= 0x60; m.put(invalid)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 2)
        self.assertEqual(m.read(21), 0)
        self.assertEqual(m.record()[43] & 0x80, 0x80)

    def test_page_selection_is_runtime_only_and_invalid_modes_are_rejected(self):
        m = self.m; before = m.record()
        m.call("Settings_Set", 23, 0, 12)
        self.assertEqual(m.call("Settings_Get", 23, 0), 12)
        self.assertEqual(m.record(), before); self.assertEqual(m.call("TestDirty"), 0)
        m.call("Settings_Set", 23, 0, 19)
        self.assertEqual(m.call("Settings_Get", 23, 0), 12)
        for field, value in [(21,3),(22,6)]: self.assertEqual(m.write(field,0,value),0)
        self.assertEqual(m.record(), before)

    def test_canonical_grid_and_panel_mapping_of_all_players_and_categories(self):
        m = self.m; ptr = 0x80406000
        m.call("TMOSD_MapReset",ptr,sum(1<<id for id in IDS))
        m.call("TMOSD_MapOwners",ptr,63)
        self.assertEqual(m.call("TMOSD_Count",ptr),114)
        self.assertEqual(m.call("TMOSD_PageCount",ptr,1),12)
        self.assertEqual(m.call("TMOSD_PageCount",ptr,2),13)
        for player in range(6):
            for slot, category in enumerate(IDS):
                key = m.call("TMOSD_Key",player,category)
                self.assertEqual(key,player*19+slot)
                self.assertEqual(m.call("TMOSD_Category",key),category)
                for mode, capacity in [(1,10),(2,9)]:
                    self.assertEqual(m.call("TestLayoutCell",ptr,key,mode,0),key//capacity)
                    self.assertEqual(m.call("TestLayoutCell",ptr,key,mode,1),key%capacity)
        for player, category in [(-1,0),(6,0),(0,2),(0,64)]:
            self.assertEqual(m.call("TMOSD_Key",player,category),0xFFFFFFFF)

    def test_new_owner_appends_without_moving_existing_slots(self):
        m = self.m; ptr = 0x80406000
        m.call("TMOSD_MapReset",ptr,(1<<0)|(1<<20));m.call("TMOSD_MapOwners",ptr,5)
        key = m.call("TMOSD_Key",2,20)
        before = [m.call("TestLayoutCell",ptr,key,1,i) for i in range(4)]
        m.call("TMOSD_MapOwners",ptr,7)
        self.assertEqual([m.call("TestLayoutCell",ptr,key,1,i) for i in range(4)],before)
        m.call("TMOSD_MapOwners",ptr,1)
        self.assertEqual(m.call("TMOSD_Count",ptr),6)  # Hidden owners retain their cells.

    def test_repeated_results_retain_positions_and_do_not_expire_in_both_styles(self):
        for mode in (1,2):
            self.m = Machine();self.m.init();self.start(mode)
            m = self.m
            wave=m.call("TestLayoutEmit",0,0,1,0);fast=m.call("TestLayoutEmit",0,20,2,20)
            m.call("TestLayoutTick",1)
            before=[m.call("TestLayoutX100",wave),m.call("TestLayoutY100",wave)]
            for frame in range(2,125):m.call("TestLayoutTick",frame)
            self.assertEqual(m.call("TestLayoutAge",wave),120)
            self.assertEqual(m.call("TestLayoutVisible",wave),1)
            self.assertIn(b"last",self.footer(wave))
            self.assertEqual(m.call("TestLayoutFreed"),0)
            m.call("TestLayoutEmit",0,20,3,20);m.call("TestLayoutTick",125)
            self.assertEqual([m.call("TestLayoutX100",wave),m.call("TestLayoutY100",wave)],before)
            self.assertEqual(m.call("TestLayoutFreed"),1)

    def test_manual_pages_do_not_follow_new_results_or_expiration(self):
        self.start(1,IDS);m=self.m
        object=m.call("TestLayoutEmit",0,16,1,16);m.call("TestLayoutTick",1)
        self.assertEqual(m.call("TestLayoutVisible",object),0)
        self.assertEqual(m.call("Settings_Get",23,0),0)
        m.call("Settings_Set",23,0,1);m.call("TestLayoutTick",2)
        self.assertEqual(m.call("TestLayoutVisible",object),1)
        for frame in range(3,130):m.call("TestLayoutTick",frame)
        self.assertEqual(m.call("Settings_Get",23,0),1)
        self.assertLessEqual(m.call("TestLayoutLiveText"),21)
        m.call("Settings_Set",23,0,18);m.call("TestLayoutTick",130)
        self.assertEqual(m.call("Settings_Get",23,0),1)  # Clamp invalid explicit page, no cycling.

    def test_pause_duplicate_ticks_and_stock_generation_preserve_new_result(self):
        self.start(2);m=self.m
        old=m.call("TestLayoutEmit",0,20,1,20);m.call("TestLayoutTick",1)
        age=m.call("TestLayoutAge",old)
        for _ in range(4):m.call("TestLayoutTick",1)
        self.assertEqual(m.call("TestLayoutAge",old),age)
        m.call("TestLayoutPause",2);m.call("TestLayoutTick",2)
        self.assertEqual(m.call("TestLayoutAge",old),age)
        m.call("TestLayoutPause",0);m.call("TestLayoutSpawn",0,7)
        fresh=m.call("TestLayoutEmit",0,20,2,20);m.call("TestLayoutTick",3)
        self.assertEqual(m.call("TestLayoutFreed"),1)
        self.assertEqual(m.call("TestLayoutVisible",fresh),1)
        self.assertNotIn(b"1f",self.footer(fresh))
        m.call("Message_LayoutClear")
        fresh=m.call("TestLayoutEmit",0,20,3,20);m.call("TestLayoutTick",0)
        self.assertEqual(m.call("TestLayoutVisible",fresh),1)
        self.assertNotIn(b"2f",self.footer(fresh))

    def test_panel_history_is_bounded_deduplicated_typed_and_cleared_for_subtypes(self):
        self.start(2);m=self.m
        for frame, result in enumerate([1,2,3,4],1):
            m.call("TestLayoutTick",frame)
            object=m.call("TestLayoutEmit",0,20,result,20);m.call("TestLayoutTick",frame)
        text=self.footer(object)
        self.assertIn(b"1f",text); self.assertIn(b"2f",text);self.assertIn(b"3f",text);self.assertIn(b"4f",text)
        self.assertIn(b"\x1bFFF000FF3f",text)  # Third timing remains yellow.
        encoded=m.native_text(text);m.native_width(encoded);m.native_subtexts([encoded])
        # Same callback update/value must not add another history item.
        object=m.call("TestLayoutEmit",0,20,4,20);m.call("TestLayoutTick",4)
        self.assertEqual(self.footer(object).count(b"4f"),1)
        object=m.call("TestLayoutEmit",0,20,1,64);m.call("TestLayoutTick",5)
        self.assertNotIn(b"2f",self.footer(object));self.assertNotIn(b"4f",self.footer(object))

    def test_all_and_cpu_overrides_preserve_human_slot_coordinates(self):
        self.start(1);m=self.m
        human=m.call("TestLayoutEmit",0,20,1,20);m.call("TestLayoutTick",1)
        before=[m.call("TestLayoutX100",human),m.call("TestLayoutY100",human)]
        m.call("Settings_Set",11,0,1);m.call("OSD_MessageGX",human,2)
        m.call("Settings_Set",11,0,0);m.call("TestLayoutTick",2)
        self.assertEqual([m.call("TestLayoutX100",human),m.call("TestLayoutY100",human)],before)
        m.call("TestCueState",2,14,1,40,100);m.call("TestCuePlayer",1,1,1)
        cpu=m.call("TestLayoutEmit",1,0,1,0);m.call("TestLayoutTick",3)
        m.call("Settings_Set",11,8,1);m.call("OSD_MessageGX",cpu,2)
        m.call("TestLayoutTick",4)
        self.assertEqual([m.call("TestLayoutX100",human),m.call("TestLayoutY100",human)],before)
        self.assertIn(b"CPU2",self.footer(cpu))

    def test_importing_old_recent_result_cannot_overwrite_a_fresh_native_result(self):
        self.start(0);m=self.m
        old=m.call("TestLayoutEmit",0,20,1,20)
        m.call("Settings_Set",21,0,2)
        fresh=m.call("TestLayoutEmit",0,20,2,20)
        self.assertEqual(m.call("Message_LayoutImport",old,0),1)
        m.call("TestLayoutTick",1)
        self.assertEqual(m.call("TestLayoutFreed"),1)
        self.assertEqual(m.call("TestLayoutVisible",fresh),1)
        self.assertIn(b"2f",self.footer(fresh));self.assertNotIn(b"1f",self.footer(fresh))

    def test_recent_imports_from_one_update_keep_queue_recency(self):
        self.start(0);m=self.m
        old=m.call("TestLayoutEmit",0,8,1,8)
        newer=m.call("TestLayoutEmit",0,8,2,64)
        m.call("Settings_Set",21,0,2)
        self.assertEqual(m.call("Message_LayoutImport",old,0),1)
        self.assertEqual(m.call("Message_LayoutImport",newer,0),1)
        m.call("TestLayoutTick",1)
        self.assertEqual(m.call("TestLayoutFreed"),1)
        self.assertIn(b"2f",self.footer(newer));self.assertNotIn(b"1f",self.footer(newer))

    def test_compact_panel_large_result_and_vertical_previous_scores(self):
        self.start(2);m=self.m
        for frame in range(1,5):
            m.call("TestLayoutTick",frame)
            object=m.call("TestLayoutEmit",0,20,frame,20);m.call("TestLayoutTick",frame)
        self.assertEqual(m.call("TestLayoutFooterMetric",object,2,2),220)
        self.assertEqual(m.call("TestLayoutFooterMetric",object,0,2),72)
        for row, expected in [(4,b"3f"),(5,b"2f"),(6,b"1f")]:
            line=bytes(m.call("TestLayoutFooterRowChar",object,row,i) for i in range(40)).split(b"\0",1)[0]
            self.assertIn(expected,line)
            self.assertEqual(m.call("TestLayoutFooterMetric",object,row,0),730)
        self.assertEqual(m.call("TestLayoutFooterMetric",object,4,1),(-28)&0xFFFFFFFF)
        self.assertEqual(m.call("TestLayoutFooterMetric",object,5,1),0)
        map_ptr=0x80406000;m.call("TMOSD_MapReset",map_ptr,sum(1<<id for id in IDS));m.call("TMOSD_MapOwners",map_ptr,1)
        for key in range(5):self.assertEqual(m.call("TestLayoutCell",map_ptr,key,1,3),1800)
        self.assertEqual(m.call("TestLayoutCell",map_ptr,5,1,3),1100)

    def test_switching_back_to_recent_releases_stable_ownership(self):
        self.start(2);m=self.m
        one=m.call("TestLayoutEmit",0,0,1,0);two=m.call("TestLayoutEmit",0,20,2,20)
        m.call("TestLayoutTick",1);m.call("Settings_Set",21,0,1);m.call("TestLayoutTick",2)
        self.assertEqual(m.call("TestLayoutVisible",one),1)
        m.call("Settings_Set",21,0,0);m.call("TestLayoutTick",3)
        self.assertEqual(m.call("TestLayoutRecent"),2)
        self.assertEqual(m.call("TestLayoutVisible",one),1)
        m.call("TestLayoutTick",4)
        self.assertEqual(m.call("TestLayoutRecent"),2)

    def test_native_editor_paging_does_not_use_the_opening_l_press_or_save_bits(self):
        m=self.m
        for id in IDS:m.write(14,id,6)
        m.call("Settings_Set",21,0,1);m.call("TestEditorInit")
        saved=m.record()
        m.call("TestEditorInput",0x40,0)  # Opening L is consumed without paging.
        self.assertEqual(m.call("Settings_Get",23,0),0)
        m.call("TestEditorInput",0x20,0)
        self.assertEqual(m.call("Settings_Get",23,0),1)
        self.assertEqual(m.record(),saved)


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
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, 0), 216)
            for age in range(1, lifetime):
                alpha = self.m.call("TMTrail_Alpha", mode, age)
                self.assertGreater(alpha, 0)
                self.assertLessEqual(alpha, 84)
            self.assertEqual(self.m.call("TMTrail_Alpha", mode, lifetime), 0)
        self.assertEqual(self.m.call("TMTrail_Alpha", 5, 0xFFFFFFFF), 84)
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
            self.assertEqual(m.call("TMTrail_DamageColor", base, 0), (base & 0xFFFFFF00)|216)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 3), (base & 0xFFFFFF00)|216)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 15), (accent & 0xFFFFFF00)|216)
            self.assertEqual(m.call("TMTrail_DamageColor", base, 100), (accent & 0xFFFFFF00)|216)
            colors = [m.call("TMTrail_DamageColor", base, d) for d in [2,3,9,12,15]]
            self.assertNotEqual(colors[2], colors[3])  # Fox nair early/late phases remain distinct.
            for color in colors:
                self.assertEqual(m.call("TMTrail_SampleAlpha", 2, 0, color), 216)
                self.assertEqual(m.call("TMTrail_SampleAlpha", 2, 1, color), 84)
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
        m.call("TestCueKind", 0, 1)
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
        self.assertEqual(m.call("OSD_MessageLine", m.call("TestWaitTag")), 2)
        self.assertEqual(m.call("OSD_MessageArgument", m.call("TestWaitTag")), 2)
        self.assertEqual(m.call("OSD_MessagePointerFirst", m.call("TestWaitTag")), 1)
        m.call("TestCueState", 0, 70, 0, 40, 100)
        m.call("TestCueMissed", 0, 6)
        m.call("TestCueState", 0, 44, 0, 40, 100)
        m.cpu.mem_write(data+0x23FC, struct.pack(">6H",14,70,65,0,0,0))
        m.cpu.mem_write(data+0x2408, struct.pack(">6H",1,10,20,0,0,0))
        m.call("TestWaitDisplay", 0)
        self.assertEqual(bytes(m.call("TestWaitLabelChar", i) for i in range(8)), b"L-cancel")
        m.call("ActionCues_Clear")
        self.assertEqual(m.call("OSDContext_MessageHitlag", 0, 8), 0)


class ShineEpisodeTests(unittest.TestCase):
    def setUp(self):
        self.m=Machine(); self.episode=0x80402000
        self.m.cpu.mem_write(self.episode,bytes(60))

    def tick(self,frame,state,hitlag=0,dead=0):
        self.m.call("TMShine_Tick",self.episode,frame,state,hitlag,dead)

    def act(self,frame,before,after,frozen=0,jump_available=1):
        self.m.call("TMShine_Before",self.episode,frame,before,frozen,jump_available)
        return self.m.call("TMShine_After",self.episode,frame,after,frozen)

    def fields(self):
        return struct.unpack(">12I",self.m.cpu.mem_read(self.episode,48))

    def test_jump_without_turn_uses_actual_loop_opportunities(self):
        self.tick(1,360);self.assertEqual(self.act(1,360,360),0)
        self.tick(2,361);self.assertEqual(self.act(2,361,361),0)
        self.tick(3,361);self.assertEqual(self.act(3,361,24),1)
        self.assertEqual(self.fields()[4:8],(2,0,0,1))
        self.assertEqual(self.act(3,361,24),0)  # No duplicate message on the same update.

    def test_timeout_at_fifteen_and_boundary_jump_or_second_turn_wins(self):
        for frame in range(1,15):self.tick(frame,361);self.assertEqual(self.act(frame,361,361),0)
        self.tick(15,361);self.assertEqual(self.act(15,361,361),3)
        self.assertEqual(self.act(15,361,361),0)
        self.m.cpu.mem_write(self.episode,bytes(60))
        for frame in range(1,15):self.tick(frame,361);self.act(frame,361,361)
        self.tick(15,361);self.assertEqual(self.act(15,361,24),1)
        self.m.cpu.mem_write(self.episode,bytes(60))
        self.tick(1,361);self.act(1,361,364)
        for frame in (2,3):self.tick(frame,364);self.act(frame,364,364)
        for frame in range(4,17):self.tick(frame,361);self.act(frame,361,361)
        self.tick(17,361);self.assertEqual(self.act(17,361,364),2)

    def test_timeout_pauses_for_hitlag_turn_and_reflection_recovery(self):
        self.tick(1,361);self.act(1,361,364)
        for frame in range(2,20):self.tick(frame,364,1);self.assertEqual(self.act(frame,364,364,1),0)
        for frame in range(20,34):self.tick(frame,361);result=self.act(frame,361,361)
        self.assertEqual(result,3)
        self.m.cpu.mem_write(self.episode,bytes(60))
        self.tick(1,361);self.act(1,361,362)
        for frame in range(2,20):self.tick(frame,362);self.assertEqual(self.act(frame,362,362),0)
        self.tick(20,361);self.assertEqual(self.act(20,361,24),1)

    def test_release_still_reports_failure_and_new_shine_cancels_pending_deadline(self):
        self.tick(1,361);self.act(1,361,363)
        for frame in range(2,15):self.tick(frame,14);self.assertEqual(self.act(frame,14,14),0)
        self.tick(15,14);self.assertEqual(self.act(15,14,14),3)
        self.tick(16,360);self.act(16,360,360)
        self.tick(17,361);self.assertEqual(self.act(17,361,24),1)

    def test_native_timeout_emits_red_fail_once_with_context(self):
        m=self.m;m.init();m.call("TestCueInit");m.write(14,8,6);m.call("TestCueKind",0,1)
        for frame in range(1,16):
            m.call("TestCueState",0,361,frame,40,100);m.call("TestContextTick",0,frame)
            m.call("TestShineBefore",0);m.call("TestShineAfter",0)
        m.call("TestStyleFormat",0,0,-15)
        text,_,raw=styled_ascii(m,1)
        self.assertEqual(text,"FAIL");self.assertIn(b"\x1bFFA2BAFFFAIL",raw)
        m.native_width(m.native_text(raw))
        m.call("TestWaitClear");m.call("TestContextTick",0,16);m.call("TestShineBefore",0);m.call("TestShineAfter",0)
        self.assertEqual(m.call("TestWaitFrame"),0)

    def test_startup_hitlag_first_turn_and_two_blocked_turn_updates(self):
        for frame in range(1,4):
            self.tick(frame,365,1);self.assertEqual(self.act(frame,365,365,1),0)
        self.tick(4,366);self.assertEqual(self.act(4,366,369),0)
        self.assertEqual(self.fields()[3:8],(3,1,1,1,0))
        for frame in [5,6]:
            self.tick(frame,369);self.assertEqual(self.act(frame,369,369),0)
            self.assertEqual(self.fields()[4],1)  # Native turn recovery is not added to delay.
        self.tick(7,366);self.assertEqual(self.act(7,366,27),1)
        self.assertEqual(self.fields()[3:8],(3,1,1,1,1))  # 3hl->1trn->1f.

    def test_second_turn_completes_once_and_keeps_separate_timing(self):
        self.tick(1,361);self.assertEqual(self.act(1,361,361),0)
        self.tick(2,361);self.assertEqual(self.act(2,361,364),0)  # First turn at 2trn.
        self.tick(3,364);self.assertEqual(self.act(3,364,364),0)
        self.tick(4,361);self.assertEqual(self.act(4,361,361),0)
        self.tick(5,361);self.assertEqual(self.act(5,361,364),2)  # Second turn at 2trn.
        self.assertEqual(self.fields()[4:8],(2,2,2,1))
        self.tick(6,361);self.assertEqual(self.act(6,361,24),0)
        self.tick(7,360);self.assertEqual(self.act(7,360,360),0)  # A new shine resets the verdict.
        self.tick(8,361);self.assertEqual(self.act(8,361,24),1)
        self.assertEqual(self.fields()[4:8],(1,0,0,1))

    def test_release_failed_jump_duplicate_frame_and_ground_air_transfer(self):
        self.tick(1,361);self.assertEqual(self.act(1,361,361),0)
        self.assertEqual(self.act(1,361,361),0)
        self.assertEqual(self.fields()[4],1)
        self.tick(2,366);self.assertEqual(self.act(2,366,366),0)
        self.assertEqual(self.fields()[4],2)
        self.tick(3,366);self.assertEqual(self.act(3,366,368),0)  # B release emits no Jump OSD.
        self.tick(4,368);self.assertEqual(self.act(4,368,27),0)  # Jump during end is not a shine cancel.
        self.tick(5,29);self.assertEqual(self.fields()[3:8],(0,3,0,0,0))  # Pending release retains failure context.

    def test_no_air_jump_left_does_not_become_late_jump_time_after_landing(self):
        for frame in range(1,5):
            self.tick(frame,366);self.assertEqual(self.act(frame,366,366,0,0),0)
        self.tick(5,361);self.assertEqual(self.act(5,361,24),1)
        self.assertEqual(self.fields()[4],5)  # Turning was available throughout the loop.
        self.assertEqual(struct.unpack(">I",self.m.cpu.mem_read(self.episode+48,4))[0],1)

    def test_turn_hitlag_and_native_loop_resumption_after_any_blocked_duration(self):
        self.tick(1,361);self.assertEqual(self.act(1,361,364),0)
        for frame in range(2,5): self.tick(frame,364,1);self.act(frame,364,364,1)
        self.tick(5,361);self.assertEqual(self.act(5,361,361),0)
        self.tick(6,361);self.assertEqual(self.act(6,361,24),1)
        self.assertEqual(self.fields()[3:6],(3,2,1))
        self.tick(2,361);self.assertEqual(self.fields()[3:8],(0,0,0,0,0))  # Rewind clears.
        self.tick(3,361,0,1);self.assertEqual(self.fields()[3:8],(0,0,0,0,0))

    def test_native_adapter_observes_turn_and_jump_after_iasa_for_fox_and_falco(self):
        for kind in [1,22]:
            m=Machine();m.init();m.call("TestCueInit");m.write(14,8,1)
            m.call("TestCueKind",0,kind);m.call("TestWaitClear")
            def before(frame,state,frozen=0):
                m.call("TestCueState",0,state,0,40,100);m.call("TestCueFrozen",0,frozen)
                m.call("TestContextTick",0,frame);m.call("TestShineBefore",0)
            def after(state):
                m.call("TestCueState",0,state,0,40,100);m.call("TestShineAfter",0)
            for frame in range(1,4): before(frame,360,1);after(360)
            before(4,361);after(364)
            self.assertEqual(m.call("TestWaitFrame"),0)  # A turn is no longer an Act OoShine result.
            before(5,364);after(364)
            before(6,361);after(24)
            self.assertEqual(m.call("TestWaitFrame"),1)
            text,colors,raw=styled_ascii(m,1)
            self.assertEqual(text,"3hl->1trn->1f")
            self.assertEqual(colors[5][1],0x00FFFFFF)
            self.assertEqual(colors[-1][1],0x00FFFFFF)
            m.native_text(raw)
            m.call("ActionCues_Clear");m.call("TestWaitClear")
            before(7,360);after(360)
            before(8,361);after(364)
            before(9,364);after(364)
            before(10,361);after(364)
            text,colors,raw=styled_ascii(m,1)
            self.assertEqual(text,"1trn->1trn")
            self.assertEqual(colors[0][1],0x00FFFFFF)
            self.assertEqual(colors[-1][1],0xFFA2BAFF)
            m.call("TestWaitClear")
            before(11,361);after(24)
            self.assertEqual(m.call("TestWaitFrame"),0)  # The two-turn result was terminal.


class OSDStyleTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def test_real_native_setter_replaces_colored_rows_without_orphaned_color_bytes(self):
        original=[b"Jump Out Of Shine",b"4f",b"Landing"]
        first=b"\x1BFFFFFFFF3hl->\x1BFFA2BAFF4f"
        second=b"\x1BFFFFFFFF3hl->\x1B00FFFFFF1trn->\x1B8DFF6EFF2f"
        # The old length iterator stops at the first inline RGB command. Its
        # next rewrite overwrites that opcode but leaves color bytes as glyphs.
        with self.assertRaises(AssertionError):
            Machine().native_rewrite_rows(original,[(1,first),(1,second)],patch_iterator=False)
        for sources,index in [(original,1),([b"Wavedash 1f",b"Angle: 20.0",b"Short Hop: 1f"],0)]:
            updates=[(index,first),(index,second),(index,b"\x1B00FFFFFF1f"),
                     (index,b"\x1BFFF000FF3trn->\x1BFFA2BAFF1trn"),(index,b"1f")]*4
            Machine().native_rewrite_rows(sources,updates)

    def test_inline_color_rgb_payload_survives_native_width_and_subtext_parsers(self):
        m=self.m
        # Cover zero bytes, 0xFF alpha in source, bright colors and all affected
        # layouts; verify actual text traversal rather than only conversion.
        rows=[]
        for source in [b"Wavedash \x1B00FFFFFF1f",b"\x1BFFFFFFFF3hl->\x1B00FFFFFF1f",
                       b"\x1BFFFFFFFF3hl->\x1BFFF000FF3trn->\x1BFFA2BAFF1trn",
                       b"\x1B8DFF6EFF2f/7f",b"\x1B000000FF1f"]:
            encoded=m.native_text(source)
            width,height=m.native_width(encoded)
            self.assertGreater(width,0)
            self.assertLess(width,500)
            self.assertGreaterEqual(height,0)
            self.assertLessEqual(height,32)
            rows.append(encoded)
        m.native_subtexts([rows[0],m.native_text(b"Angle: 20.0"),m.native_text(b"Short Hop: 1f")])
        m.native_subtexts([m.native_text(b"Act OoWait"),m.native_text(b"Landing"),rows[1]])
        m.native_subtexts([m.native_text(b"Jump Out Of Shine"),rows[2]])
        color_only=m.native_text(b"\x1B00FFFFFF")
        self.assertEqual(color_only,b"\x0c\x00\xff\xff")
        # The previous 4-byte payload leaves alpha interpreted as a glyph and
        # fails at the native text-width parser, exactly the regression fixed.
        old=b"\x0c\xff\xff\xff\xff"+m.native_text(b"1f")
        with self.assertRaises(UcError): m.native_width(old)

    def test_wavedash_top_row_is_normal_size_and_a_single_centered_row(self):
        m = self.m; m.init(); m.write(14,0,1)
        for hitlag in [0,3]:
            m.call("TestStyleInit",0,2,0,1);m.call("TestStyleFormat",hitlag,1,-30)
            text,colors,raw=styled_ascii(m,0)
            self.assertEqual(text,"Wavedash "+("3hl->" if hitlag else "")+"2f")
            self.assertEqual(m.call("TestStyleScale100",0),100)
            self.assertEqual(m.call("TestStyleX",0),0)
            self.assertEqual(m.call("TestStyleY",0),0xFFFFFFE2)
            self.assertEqual(m.call("TestStyleTimingLine"),0)
            self.assertEqual(m.call("TestStylePrefix"),0xFFFFFFFF)
            self.assertEqual(colors[-1][1],0x8DFF6EFF)
            self.assertEqual(m.call("TestStyleEncoded"),1)
            m.native_text(raw)  # Exercise the actual emitted converter hook, not a mock.

    def test_lcancel_prefixed_result_keeps_window_outcome_and_measurement(self):
        m=self.m;m.init();m.write(14,1,1)
        m.call("TestStyleInit",1,4,1,1);m.call("TestStyleFormat",7,0,-30)
        text,colors,raw=styled_ascii(m,1)
        self.assertEqual(text,"7hl->4f/7f")
        self.assertEqual(colors[0][1],0xFFFFFFFF)
        self.assertEqual(colors[-1][1],0xFFA2BAFF)
        self.assertEqual(m.call("TestStyleTimingLine"),1)
        self.assertEqual(m.call("TestStylePrefix"),0xFFFFFFFF)
        self.assertEqual(m.call("TestStyleX",1),0)
        self.assertEqual(m.call("TestStyleScale100",1),100)
        m.native_text(raw)

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
        for frame, expected in [(0, 0xFFA2BAFF), (1, 0x00FFFFFF), (2, 0x8DFF6EFF), (3, 0xFFF000FF), (4, 0xFFA2BAFF), (99, 0xFFA2BAFF)]:
            self.assertEqual(self.m.call("OSD_TimingColor", frame), expected)
        expected = [0, 0xFFFFFFFF, 0xFF4646FF, 0x8DFF6EFF, 0x4691FFFF, 0xFFF000FF, 0x00FFFFFF, 0xFF50FFFF]
        for index, rgba in enumerate(expected):
            self.assertEqual(self.m.call("OSD_PaletteColor", index), rgba)
        self.assertEqual(self.m.call("OSD_PaletteColor", 99), 0xFFFFFFFF)

    def test_hop_colors_follow_hop_type_and_exact_first_frame(self):
        for frame in [0,1,2,3,20]:
            self.assertEqual(self.m.call("OSD_WavedashHopColor",1,frame),
                             0x00FFFFFF if frame == 1 else 0x8DFF6EFF)
            self.assertEqual(self.m.call("OSD_WavedashHopColor",0,frame),0xFFA2BAFF)

    def test_real_wavedash_assembly_colors_the_printed_hop_count_not_wavedash_timing(self):
        for wave,held,short,expected in [(1,2,1,0x8DFF6EFF),(2,1,1,0x00FFFFFF),
                                         (1,1,0,0xFFA2BAFF),(3,2,0,0xFFA2BAFF)]:
            m=Machine();m.init();m.call("TestCueInit");m.write(14,0,1)
            m.call("TestCueState",0,42,0,40,100)
            data=m.call("TestCueData",0);obj=m.call("TestCueObject",0)
            m.call("TestStyleInit",0,-1,0,1)
            m.cpu.mem_write(data+0x2408,struct.pack(">H",wave))
            m.cpu.mem_write(data+0x2428,struct.pack(">fI",0.0,short))
            m.cpu.mem_write(data+0x685,bytes([held]))
            m.cpu.mem_write(data+0x2438,b"\x04\0")  # One past input-log update since jump release.
            m.cpu.reg_write(gpr(13),0x804D6D5C)
            m.cpu.mem_write(0x80300000-0x6758,struct.pack(">Q",0x4330000080000000))
            m.cpu.mem_write(0x80300000-0x3D10,struct.pack(">f",0.017453292))
            m.cpu.mem_write(0x80005510,bytes.fromhex("386000004e800020"))  # Main fighter.
            m.cpu.mem_write(0x80099D80,bytes.fromhex("4e800020"))  # Return after the displaced instruction.
            address=m.symbols["Text_SetColor"]
            m.cpu.mem_write(0x803A74F0,struct.pack(">4I",0x3D800000|(address>>16),
                0x618C0000|(address&65535),0x7D8903A6,0x4E800420))
            for slot,name in [(21,"Message_Display"),(35,"OSD_WavedashHopColor")]:
                m.cpu.mem_write(0x80300000-200+slot*4,struct.pack(">I",m.symbols[name]))
            m.symbols["NativeWaveProducer"]=m.install_hook(0x80099D7C,0x80481000)
            m.call("NativeWaveProducer",obj)
            self.assertEqual(m.call("TestWaveFrame"),wave)
            self.assertEqual(m.call("TestWaveHop"),held)
            self.assertEqual(m.call("TestStyleColor",2),expected)

    def test_cpu_override_hides_owned_messages_live_and_preserves_humans_and_general_feedback(self):
        m=self.m; m.init(); m.call("TestCueInit")
        m.write(14,20,6)
        m.call("TestCuePlayer",2,1,1)  # CPU is identified by player type, not port number.
        m.call("TestStyleInit",20,3,1,1); m.call("TestStyleQueue",2)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),0)
        self.assertEqual(m.call("TestStyleColor",1),0xFFF000FF)
        m.write(11,8,1)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),1)
        self.assertEqual(m.call("TestStyleBackgrounds"),1)
        self.assertEqual(m.read(14,20),6)
        m.call("TestStyleQueue",0); m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),0)
        m.call("TestStyleInit",-1,-1,1,1); m.call("TestStyleQueue",2)
        m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),1)  # Untagged CPU-owned messages are covered too.
        m.call("TestStyleQueue",6); m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),0)
        m.call("TestStyleQueue",2); m.write(11,8,0); m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),0)
        m.call("TestStyleInit",20,3,1,1); m.write(11,0,1); m.call("TestStyleDraw")
        self.assertEqual(m.call("TestStyleHidden"),1)  # ALL override still independently covers humans.

    def test_hitlag_and_turn_colors_share_one_positioned_row(self):
        m=self.m;m.init();m.write(14,8,1)
        for first,result,second in [(1,1,0),(2,3,0),(3,1,1)]:
            m.call("TestStyleInit",8,result,1,1);m.call("TestStyleTurn",first,second)
            m.call("TestStyleFormat",3,0,-15)
            text,colors,raw=styled_ascii(m,1)
            self.assertEqual(text,f"3hl->{first}trn->{result}"+("trn" if second else "f"))
            self.assertEqual(colors[0][1],0xFFFFFFFF)
            self.assertEqual(colors[5][1],m.call("OSD_TimingColor",first))
            self.assertEqual(colors[-1][1],0xFFA2BAFF if second else m.call("OSD_TimingColor",result))
            self.assertEqual(m.call("TestStyleX",1),0)
            self.assertEqual(m.call("TestStyleY",1),15)
            self.assertEqual(m.call("TestStylePrefix"),0xFFFFFFFF)
            encoded=m.native_text(raw)
            self.assertIn(b"\x0c\xff\xff\xff",encoded)
            self.assertIn(b"\x0c"+colors[-1][1].to_bytes(4,"big")[:3],encoded)

    def test_source_first_timing_tag_reads_string_and_integer_with_their_actual_types(self):
        m=self.m
        source=0x80402000; m.cpu.mem_write(source,b"Landing\0")
        tag=m.call("OSD_MessageTag",5,16,2,2,0)|(1<<22)
        self.assertEqual(m.call("TestTimingArgument",tag,0,0,0,source,3),3)
        self.assertEqual(m.call("OSD_MessageLine",tag),2)

    def test_non_one_best_frame_does_not_change_measurement(self):
        tag = self.m.call("OSD_MessageTag", 8, 8, 1, 1, 0) | (4 << 23)
        self.assertEqual(self.m.call("OSD_MessageBestFrame", tag), 5)
        self.assertEqual(self.m.call("TestTimingArgument", tag, 0, 0, 0, 5), 5)
        for frame, color in [(4, 0xFFA2BAFF), (5, 0x00FFFFFF), (6, 0x8DFF6EFF), (7, 0xFFF000FF), (8, 0xFFA2BAFF)]:
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

    def test_cpu_override_editor_row_and_saved_bit_are_independent(self):
        m=self.m
        m.write(14,20,6); m.write(15,29,1)  # An unknown old enable bit must survive this row allocation.
        reserved=bytearray(m.record()); reserved[40:44]=b"\xf0ABC"; m.put(reserved)
        m.call("TestEditorInit"); m.call("TestEditorInput",0x200,19)
        self.assertEqual(m.read(11,8),1)
        self.assertEqual(m.record()[40:44],b"\xf4ABC")
        self.assertEqual(m.read(15,29),1)
        self.assertEqual(m.read(14,20),6)
        self.assertEqual(m.call("TestEditorHidden",19),0)
        m.call("TestEditorAnimate",19); m.call("TestSettingsEditorWrite",16,m.call("TestEditorCache",19))
        saved=m.record(); fresh=Machine(); fresh.put(saved); fresh.call("TestEditorInit")
        self.assertEqual(fresh.call("Settings_Get",11,8),1)
        for row,label in [(19,"OVERRIDE CPU OSDS OFF"),(20,"OVERRIDE ALL OSDS OFF")]:
            self.assertEqual(''.join(chr(fresh.call("TestEditorLabelChar",row,i)) for i in range(len(label))),label)
        self.assertEqual(fresh.record(),saved)

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
        for row in [21, 29, 65535]:
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
        expected = IDS + [29,6,255,2,4,7,17,11,23,25]
        for i, id in enumerate(IDS):
            m.write(14, id, (i % 7) + 1)
        for flag in range(8):
            m.write(11, flag, flag & 1)
        m.write(15, 15, 1)  # Reserved event preference survives menu exit.
        m.call("TestEditorInit")
        for row, native in enumerate(physical):
            self.assertEqual(m.call("TMSettings_EditorID", native), expected[row])
            self.assertEqual(m.call("TestEditorHidden", row), int(row == 21))
            self.assertEqual(m.read(17, native), m.call("TestEditorCache", row))
            self.assertEqual(m.call("TestSettingsEditorRow", native), m.call("TestEditorCache", row))
        label = "OVERRIDE ALL OSDS OFF"
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
        self.assertEqual(m.call("TestEditorLabelChar", 21, 0), 0)
        self.assertEqual(m.call("TestEditorLabelChar", 20, 0), ord("O"))


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

    def test_version_one_migration_clears_new_flags_and_event_initialization(self):
        m = self.m
        old = bytearray(m.record())
        old[10] = 0x7F
        old[40:44] = b"\xFFABC"
        m.put(old)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
        self.assertEqual(m.record()[10], 0xFF)
        self.assertEqual(m.record()[40:44], b"\xF0AB\x03")
        self.assertEqual(m.read(11, 6), 0)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        m.write(16, 17, 1)
        self.assertEqual(m.record()[40:44], b"\xF1AB\x03")
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

    def test_version_two_migration_preserves_turnrun_and_clears_event_initialization(self):
        m = self.m
        old = bytearray(m.record())
        old[10] = 0xBF
        old[40:44] = b"\xFFABC"
        m.put(old)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 1)
        self.assertEqual(m.record()[10], 0xFF)
        self.assertEqual(m.record()[40:44], b"\xF1AB\x03")
        self.assertEqual(m.read(11, 6), 1)
        self.assertEqual(m.read(11, 7), 0)
        self.assertEqual(m.call("TMSettings_Prepare", RECORD, 1), 0)
        m.call("TestEditorInit")
        m.call("TestEditorInput", 0x200, 28)  # Protection at final grouped row.
        m.call("TestEditorAnimate", 28)
        m.write(16, 25, m.call("TestEditorCache", 28))
        self.assertEqual(m.record()[40:44], b"\xF3AB\x03")
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

    def test_egg_random_choices_change_only_on_reset_and_survive_restore_recreate(self):
        m=self.m; placement,options=0x80402000,0x80402100
        m.cpu.mem_write(placement,bytes(8))
        m.cpu.mem_write(options,struct.pack(">5i",2,20,1,10,35))
        m.call("LdshEgg_Choose",placement,options,1,1,7)
        saved=bytes(m.cpu.mem_read(placement,8))
        self.assertEqual(struct.unpack(">2i",saved),(1,17))
        for contact in range(50):
            m.call("LdshEgg_Choose",placement,options,0,contact%2,contact)
            self.assertEqual(bytes(m.cpu.mem_read(placement,8)),saved)
        m.call("LdshEgg_Choose",placement,options,1,0,20)
        self.assertEqual(struct.unpack(">2i",m.cpu.mem_read(placement,8)),(0,30))
        m.cpu.mem_write(placement,saved)  # Event-data restore carries the previous choice.
        m.call("LdshEgg_Choose",placement,options,0,0,20)
        self.assertEqual(bytes(m.cpu.mem_read(placement,8)),saved)
        m.cpu.mem_write(options,struct.pack(">5i",0,25,0,10,35))
        m.call("LdshEgg_Choose",placement,options,1,1,7)
        self.assertEqual(struct.unpack(">2i",m.cpu.mem_read(placement,8)),(0,25))


if __name__ == "__main__":
    unittest.main()
