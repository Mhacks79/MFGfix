"""List every call to a given function in MfgConsole.dll with the few instructions
before it (string pushes resolved). py -3.11 -I mccalls.py <dll> <target-hex> [n]"""
import sys, capstone, pefile
pe = pefile.PE(sys.argv[1]); base = pe.OPTIONAL_HEADER.ImageBase; img = pe.get_memory_mapped_image()
target = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 6
text = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
lo = base + text.VirtualAddress; code = img[text.VirtualAddress:text.VirtualAddress + text.Misc_VirtualSize]
def cstr(v):
    o = v - base
    if 0 <= o < len(img):
        s = img[o:o+60].split(b'\0')[0]
        if len(s) >= 2 and all(32 <= c < 127 for c in s): return s.decode()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
ins = list(md.disasm(code, lo))
for i, x in enumerate(ins):
    if x.mnemonic == 'call' and x.op_str == hex(target):
        ctx = []
        for y in ins[max(0, i-n):i]:
            t = '%s %s' % (y.mnemonic, y.op_str)
            for tok in y.op_str.replace(",", " ").replace("[", " ").replace("]", " ").split():
                if tok.startswith('0x'):
                    s = cstr(int(tok, 16))
                    if s: t += ' "%s"' % s
            ctx.append(t)
        print('%08x: %s' % (x.address, ' | '.join(ctx)))
