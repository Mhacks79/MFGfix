"""Linear disassembly of MfgConsole.dll (image base 0x10000000) with string and
call annotations, so the member names handed to GFxValue setters in registers are
visible (the decompiler drops them).

    py -3.11 -I mcdis.py <dll> <start-hex> [end-hex]
A function ends at the first ret when no end is given and no later jump comes back.
"""
import sys
import capstone
import pefile

pe = pefile.PE(sys.argv[1])
base = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()
start = int(sys.argv[2], 16)
end = int(sys.argv[3], 16) if len(sys.argv) > 3 else None


def cstr(va):
    off = va - base
    if off < 0 or off >= len(img):
        return None
    s = img[off:off + 80].split(b'\0')[0]
    if len(s) >= 2 and all(32 <= c < 127 for c in s):
        return s.decode()
    return None


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False
code = img[start - base:(end or start + 0x4000) - base]
far = start
for ins in md.disasm(code, start):
    note = ''
    for tok in ins.op_str.replace(',', ' ').replace('[', ' ').replace(']', ' ').split():
        if tok.startswith('0x'):
            try:
                v = int(tok, 16)
            except ValueError:
                continue
            s = cstr(v)
            if s:
                note += '  ; "%s"' % s
            if ins.mnemonic.startswith('j') and v > far:
                far = v
    print('%08x  %-7s %s%s' % (ins.address, ins.mnemonic, ins.op_str, note))
    if end is None and ins.mnemonic in ('ret', 'retn') and ins.address >= far:
        break
