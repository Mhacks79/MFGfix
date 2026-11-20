"""Proves the 1.0.13 Mfg Console replacement tests both ways: every mutant puts one bug
back into the plugin source, rebuilds the offline harness and must make it FAIL. The
source is restored after each one, and a clean rebuild must pass at the end.

    py -3.11 -I mutate_legacy.py [name-filter...]
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MSBUILD = os.environ.get('MSBUILD', 'MSBuild.exe')   # full path in MSBUILD when it is not on PATH

MUTANTS = [
    ('strFormID upper case', 'src/LegacyCore.h', 'constexpr char kHexDigits[] = "0123456789abcdef";',
     'constexpr char kHexDigits[] = "0123456789ABCDEF";'),
    ('percent without +0.5', 'src/LegacyCore.h', 'static_cast<double>(value) * 100.0 + 0.5;',
     'static_cast<double>(value) * 100.0;'),
    ('expression takes zero entries', 'src/LegacyCore.h', 'if (values[i] > 0.0f) return', 'if (values[i] >= 0.0f) return'),
    ('call site patched over a foreign call', 'src/Legacy.cpp',
     '    if (site + 5 + rel != expected) return false;', '    (void)expected;'),
    ('zero phoneme written as zero', 'src/Legacy.cpp',
     'WriteFaceValue(face, 3, id, bits ? bits : 0x33D6BF95u);', 'WriteFaceValue(face, 3, id, bits);'),
    ('reset leaves the arrays', 'src/Legacy.cpp', '    if (face) ClearFaceForReset(face);\n', ''),
    ('overrides not mapped through MAST', 'src/LegacyCore.h',
     '            const std::uint32_t id = GlobalID(h.formID, index, self);',
     '            const std::uint32_t id = h.formID;'),
    ('BODT read at the part mask (original bug)', 'src/LegacyCore.h',
     '(std::uint8_t(body[at + 4]) & 0x10)', '(std::uint8_t(body[at]) & 0x10)'),
    ('modName from the reference', 'src/Legacy.cpp',
     '    UInt32 modIndex = baseID ? baseID >> 24 : 0xFF;', '    UInt32 modIndex = 0xFF;'),
    ('name not quoted', 'src/Legacy.cpp', 'name = std::string("\'") + s + "\'";', 'name = s;'),
    ('blink never stops', 'src/Legacy.cpp',
     '    InterlockedExchange(&blinkResetPending, 1);   // zeroed on the face thread (0x10003A90)\n', ''),
    ('extra info ignores its switch', 'src/Legacy.cpp', '    if (!extraInfo || !(number', '    if (!(number'),
    ('plain verbs not routed to the fixed engine path', 'src/main.cpp',
     ' || MfgLE::Legacy::callSites==7)', ')'),
    ('leveled base ignored', 'src/Legacy.cpp', '        if (IsForm(leveled)) baseID = FormIDOf(leveled);', ''),
    ('console write loses to a running transition', 'src/Engine.cpp',
     '            if(channel!=-2) Forget(data,channel);', ''),
    ('expression row read from keyframe 1', 'src/Engine.cpp',
     'if(expressionOK) memcpy(expression,data->keyFrames[5].values', 'if(expressionOK) memcpy(expression,data->keyFrames[1].values'),
    ('phoneme id not bounded (both guards)', [
        ('src/Legacy.cpp', '    if (id >= 16 || !face) return;\n    WriteFaceValue(face, 3', '    if (!face) return;\n    WriteFaceValue(face, 3'),
        ('src/Engine.cpp', '    if(id>=count) return;\n', '')]),
    ('active effect inactive flag ignored', 'src/Legacy.cpp', 'Bool(&obj, "inactive", (fx->flags & 0x8000) == 0x8000);',
     'Bool(&obj, "inactive", false);'),
    ('actor values read as base for current', 'src/Legacy.cpp',
     'Number(&obj, "current", actor->actorValueOwner.GetCurrent(av));', 'Number(&obj, "current", actor->actorValueOwner.GetBase(av));'),
    ('form type read at the SE offset +0x14', 'src/Legacy.cpp', 'Get(form, 0x12, t); return t; }', 'Get(form, 0x14, t); return t; }'),
    ('right hand not listed', 'src/Legacy.cpp', 'for (unsigned offset : {0x6Cu, 0x68u})', 'for (unsigned offset : {0x6Cu})'),
]


def build_and_run():
    b = subprocess.run([MSBUILD, os.path.join(ROOT, 'Harness.vcxproj'), '/p:Configuration=Release', '/p:Platform=Win32',
                        '/m', '/v:quiet', '/nologo'], capture_output=True, text=True, errors='replace')
    if b.returncode != 0:
        return 'build failed', b.stdout[-600:]
    exe = os.path.join(ROOT, 'build', 'tests', 'harness.exe')
    try:
        r = subprocess.run([exe], cwd=os.path.dirname(exe), capture_output=True, text=True, errors='replace', timeout=300)
    except subprocess.TimeoutExpired:
        return 'timeout', ''
    tail = [l for l in r.stdout.splitlines() if 'checks,' in l]
    return r.returncode, (tail[-1] if tail else r.stdout[-200:].replace('\n', ' '))


def main():
    only = sys.argv[1:]
    caught = total = 0
    for entry in MUTANTS:
        name = entry[0]
        edits = entry[1] if isinstance(entry[1], list) else [entry[1:]]
        if only and not any(o in name for o in only):
            continue
        total += 1
        saved = {}
        try:
            for rel, old, new in edits:
                path = os.path.join(ROOT, rel)
                if path not in saved:
                    saved[path] = open(path, 'rb').read()
                text = open(path, 'rb').read().decode('utf-8-sig').replace('\r\n', '\n')
                if text.count(old) != 1:
                    print('MUTANT NOT APPLICABLE: %s (%s)' % (name, rel))
                    sys.exit(2)
                open(path, 'w', encoding='utf-8', newline='').write(text.replace(old, new))
            rc, tail = build_and_run()
        finally:
            for path, original in saved.items():
                open(path, 'wb').write(original)
        ok = rc != 0 and rc != 'build failed'
        caught += ok
        print('%-6s %-52s rc=%s  %s' % ('caught' if ok else 'MISSED', name, rc, tail))
    rc, tail = build_and_run()
    print('restored: rc=%s  %s' % (rc, tail))
    print('%d/%d mutants caught' % (caught, total))
    sys.exit(0 if caught == total and rc == 0 else 1)


if __name__ == '__main__':
    main()
