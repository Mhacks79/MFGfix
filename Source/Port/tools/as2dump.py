"""Minimal SWF (CWS/FWS) + ActionScript 2 disassembler, enough to read how console.swf
uses skse.plugins.MfgConsole: every DoAction / DoInitAction / DefineFunction body is
printed as a flat action list with constant-pool strings resolved.

    py -3.11 -I as2dump.py <file.swf> [filter-substring]
"""
import struct
import sys
import zlib

data = open(sys.argv[1], 'rb').read()
flt = sys.argv[2] if len(sys.argv) > 2 else None
if data[:3] == b'CWS':
    body = zlib.decompress(data[8:])
else:
    body = data[8:]
# skip RECT + frame rate + frame count
nbits = body[0] >> 3
rect_bytes = (5 + 4 * nbits + 7) // 8
pos = rect_bytes + 4

NAMES = {0x04: 'NextFrame', 0x06: 'Play', 0x07: 'Stop', 0x0A: 'Add', 0x0B: 'Subtract', 0x0C: 'Multiply', 0x0D: 'Divide',
         0x0E: 'Equals', 0x0F: 'Less', 0x10: 'And', 0x11: 'Or', 0x12: 'Not', 0x17: 'Pop', 0x1C: 'GetVariable',
         0x1D: 'SetVariable', 0x21: 'StringAdd', 0x26: 'Trace', 0x3A: 'Delete', 0x3B: 'Delete2', 0x3C: 'DefineLocal',
         0x3D: 'CallFunction', 0x3E: 'Return', 0x3F: 'Modulo', 0x40: 'NewObject', 0x41: 'DefineLocal2', 0x42: 'InitArray',
         0x43: 'InitObject', 0x44: 'TypeOf', 0x47: 'Add2', 0x48: 'Less2', 0x49: 'Equals2', 0x4A: 'ToNumber', 0x4B: 'ToString',
         0x4C: 'PushDuplicate', 0x4E: 'GetMember', 0x4F: 'SetMember', 0x50: 'Increment', 0x51: 'Decrement',
         0x52: 'CallMethod', 0x53: 'NewMethod', 0x54: 'InstanceOf', 0x55: 'Enumerate2', 0x60: 'BitAnd', 0x67: 'Greater',
         0x88: 'ConstantPool', 0x96: 'Push', 0x99: 'Jump', 0x9D: 'If', 0x9B: 'DefineFunction', 0x8E: 'DefineFunction2',
         0x87: 'StoreRegister', 0x94: 'With', 0x9F: 'GotoFrame2'}


def actions(buf, start, end, pool, depth=0, out=None):
    p = start
    while p < end:
        code = buf[p]
        p += 1
        if code == 0:
            break
        length = 0
        payload = b''
        if code >= 0x80:
            length = struct.unpack_from('<H', buf, p)[0]
            p += 2
            payload = buf[p:p + length]
        name = NAMES.get(code, 'op%02X' % code)
        text = name
        if code == 0x88:
            n = struct.unpack_from('<H', payload, 0)[0]
            items = payload[2:].split(b'\0')[:n]
            pool[:] = [i.decode('latin1') for i in items]
            text = 'ConstantPool(%d)' % n
        elif code == 0x96:
            q, vals = 0, []
            while q < len(payload):
                t = payload[q]; q += 1
                if t == 0:
                    s = payload[q:payload.index(b'\0', q)]; q += len(s) + 1; vals.append('"%s"' % s.decode('latin1'))
                elif t == 1: vals.append(str(struct.unpack_from('<f', payload, q)[0])); q += 4
                elif t == 2: vals.append('null')
                elif t == 3: vals.append('undefined')
                elif t == 4: vals.append('r%d' % payload[q]); q += 1
                elif t == 5: vals.append(str(bool(payload[q]))); q += 1
                elif t == 6:
                    lo, hi = struct.unpack_from('<II', payload, q); q += 8
                    vals.append(str(struct.unpack('<d', struct.pack('<II', hi, lo))[0]))
                elif t == 7: vals.append(str(struct.unpack_from('<i', payload, q)[0])); q += 4
                elif t == 8: vals.append('"%s"' % (pool[payload[q]] if payload[q] < len(pool) else '?')); q += 1
                elif t == 9:
                    i = struct.unpack_from('<H', payload, q)[0]; q += 2
                    vals.append('"%s"' % (pool[i] if i < len(pool) else '?'))
                else:
                    break
            text = 'Push ' + ', '.join(vals)
        elif code in (0x9B, 0x8E):
            fname = payload[:payload.index(b'\0')].decode('latin1')
            q = len(fname) + 1
            nparams = struct.unpack_from('<H', payload, q)[0]; q += 2
            params = []
            if code == 0x8E:
                q += 3  # register count + flags
                for _ in range(nparams):
                    q += 1
                    s = payload[q:payload.index(b'\0', q)]; q += len(s) + 1; params.append(s.decode('latin1'))
            else:
                for _ in range(nparams):
                    s = payload[q:payload.index(b'\0', q)]; q += len(s) + 1; params.append(s.decode('latin1'))
            codesize = struct.unpack_from('<H', payload, q)[0]
            out.append('  ' * depth + 'function %s(%s) {' % (fname, ', '.join(params)))
            body_start = p + length
            actions(buf, body_start, body_start + codesize, list(pool), depth + 1, out)
            out.append('  ' * depth + '}')
            p = body_start + codesize
            continue
        elif code in (0x99, 0x9D):
            text = '%s %+d' % (name, struct.unpack_from('<h', payload, 0)[0])
        elif code == 0x87:
            text = 'StoreRegister r%d' % payload[0]
        out.append('  ' * depth + text)
        p += length
    return out


lines = []
while pos < len(body):
    rec = struct.unpack_from('<H', body, pos)[0]; pos += 2
    tag, length = rec >> 6, rec & 0x3F
    if length == 0x3F:
        length = struct.unpack_from('<I', body, pos)[0]; pos += 4
    if tag in (12, 59):  # DoAction, DoInitAction
        start = pos + (2 if tag == 59 else 0)
        lines.append('=== tag %d at %d' % (tag, pos))
        actions(body, start, pos + length, [], 0, lines)
    elif tag == 39:  # DefineSprite: nested tags
        p2 = pos + 4
        while p2 < pos + length:
            r2 = struct.unpack_from('<H', body, p2)[0]; p2 += 2
            t2, l2 = r2 >> 6, r2 & 0x3F
            if l2 == 0x3F:
                l2 = struct.unpack_from('<I', body, p2)[0]; p2 += 4
            if t2 == 12:
                lines.append('=== sprite DoAction at %d' % p2)
                actions(body, p2, p2 + l2, [], 0, lines)
            p2 += l2
    elif tag == 0:
        break
    pos += length

text = '\n'.join(lines)
if flt:
    blocks = text.split('\n')
    keep = [i for i, l in enumerate(blocks) if flt in l]
    shown = set()
    for i in keep:
        for j in range(max(0, i - 25), min(len(blocks), i + 40)):
            shown.add(j)
    print('\n'.join(blocks[j] for j in sorted(shown)))
else:
    sys.stdout.buffer.write(text.encode("utf-8", "replace"))
