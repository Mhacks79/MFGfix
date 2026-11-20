#pragma once
// Engine-free pieces of the Mfg Console replacement (1.0.13). Everything here is what
// MfgConsole.dll (sha 25194ce2) does, read out of its code with Ghidra/capstone, written
// so the offline harness can run it against hand-built plugin files and numbers.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

namespace MfgLE { namespace Legacy {

// GetObjectData strFormID / strBaseID: _itoa(id,16) appended to "00000000", last 8 kept.
// Lower case, exactly like the original (0x100108E3 = _itoa_s radix 16).
constexpr char kHexDigits[] = "0123456789abcdef";
inline void StrID(std::uint32_t id, char out[9]) {
    for (int i = 7; i >= 0; --i) { out[i] = kHexDigits[id & 0xF]; id >>= 4; }
    out[8] = 0;
}
// Phoneme / modifier / expression percentages: fld value; fmul 100.0; fadd 0.5; _ftol2.
inline int Percent(float value) {
    const double scaled = static_cast<double>(value) * 100.0 + 0.5;
    if (!(scaled > -2147483648.0 && scaled < 2147483647.0)) return 0;
    return static_cast<int>(scaled);
}
// The expression shown is the first strictly positive entry (fldz; fcom; jnp).
inline int FirstPositive(const float* values, unsigned count) {
    for (unsigned i = 0; i < count; ++i) if (values[i] > 0.0f) return static_cast<int>(i);
    return -1;
}
inline unsigned TypeIndex(unsigned formType) { return formType > 0x8C ? 0x8C : formType; }

// Player blink, timer LAB_100030D0 (30 ms period): counter-1; below zero it is re-armed
// with rand()%50*4+100; while it is under 5 the blink modifiers get a triangle 0..1..0.
inline int BlinkStep(int counter, int random) {
    --counter;
    if (counter < 0) counter = (random % 50) * 4 + 100;
    return counter;
}
inline bool BlinkValue(int counter, float& value) {
    if (counter >= 5) return false;
    const double c = counter;
    value = static_cast<float>(counter > 3 ? 1.0 - (c - 3.0) / 3.0 : c / 3.0);
    return true;
}

// ---------------------------------------------------------------------------------------
// Plugin file walking. MfgConsole reads every loaded plugin from "data\" twice over:
// FUN_100035B0 (which mods override a record: refrOverwrite / baseOverwrite) and
// GetModItems (new playable ARMO / WEAP records for "getmoditem"). Same walk here, with
// two corrections: master indices are mapped through the plugin's own MAST list (the
// original compared file-local ids with load-order ids), and the ARMO non-playable test
// reads the BODT flags byte, not the first byte of the body part mask.
struct Source {
    virtual ~Source() {}
    virtual bool Read(void* buffer, std::uint32_t size) = 0;
    virtual bool Skip(std::uint32_t size) = 0;
};
class MemorySource : public Source {
    const unsigned char* data; std::size_t size, at = 0;
public:
    MemorySource(const void* bytes, std::size_t length) : data(static_cast<const unsigned char*>(bytes)), size(length) {}
    bool Read(void* buffer, std::uint32_t n) override {
        if (n > size - at) return false;
        std::memcpy(buffer, data + at, n); at += n; return true;
    }
    bool Skip(std::uint32_t n) override { if (n > size - at) return false; at += n; return true; }
};
class FileSource : public Source {
    FILE* file = nullptr;
public:
    explicit FileSource(const char* path) { if (fopen_s(&file, path, "rb")) file = nullptr; }
    ~FileSource() override { if (file) fclose(file); }
    bool Open() const { return file != nullptr; }
    bool Read(void* buffer, std::uint32_t n) override { return file && fread(buffer, 1, n, file) == n; }
    bool Skip(std::uint32_t n) override { return file && _fseeki64(file, n, SEEK_CUR) == 0; }
};

inline std::uint32_t Tag(const char* s) {
    return std::uint32_t(std::uint8_t(s[0])) | std::uint32_t(std::uint8_t(s[1])) << 8 |
           std::uint32_t(std::uint8_t(s[2])) << 16 | std::uint32_t(std::uint8_t(s[3])) << 24;
}
struct RecordHeader { std::uint32_t type, size, flags, formID, stamp, version; };
static_assert(sizeof(RecordHeader) == 24, "Skyrim record header");
constexpr std::uint32_t kCompressed = 0x00040000;
constexpr std::uint32_t kNonPlayableRecord = 0x00000004;

// Reads TES4 and returns the master names in file order. False on a broken header.
inline bool ReadMasters(Source& in, std::vector<std::string>& masters) {
    RecordHeader h;
    if (!in.Read(&h, sizeof(h)) || h.type != Tag("TES4") || h.size > (1u << 24)) return false;
    std::vector<char> body(h.size);
    if (h.size && !in.Read(body.data(), h.size)) return false;
    std::size_t at = 0;
    while (at + 6 <= body.size()) {
        std::uint32_t type; std::uint16_t size;
        std::memcpy(&type, &body[at], 4); std::memcpy(&size, &body[at + 4], 2); at += 6;
        if (size > body.size() - at) return false;
        if (type == Tag("MAST")) masters.emplace_back(&body[at], strnlen(&body[at], size));
        at += size;
    }
    return true;
}
// Walks every record header after TES4. The visitor gets (header, source) and must either
// consume exactly header.size bytes and return true, or consume nothing and return false.
template <class Visitor>
inline bool WalkRecords(Source& in, Visitor&& visit) {
    for (;;) {
        RecordHeader h;
        if (!in.Read(&h.type, 4)) return true;  // clean end of file
        if (!in.Read(&h.size, 20)) return false;
        if (h.type == Tag("GRUP")) continue;      // enter the group: its records follow
        if (!visit(h, in) && !in.Skip(h.size)) return false;
    }
}
// Local file id -> load-order id through the plugin's MAST list. 0xFF = not loaded.
inline std::uint32_t GlobalID(std::uint32_t local, const std::vector<std::uint8_t>& masterIndex, std::uint8_t self) {
    const std::uint32_t top = local >> 24;
    const std::uint8_t index = top < masterIndex.size() ? masterIndex[top] : self;
    if (index == 0xFF) return 0;
    return (std::uint32_t(index) << 24) | (local & 0x00FFFFFF);
}

struct Override { std::uint32_t formID; std::uint8_t file; };
inline bool operator<(const Override& a, const Override& b) {
    return a.formID != b.formID ? a.formID < b.formID : a.file < b.file;
}
// One plugin's contribution to the override table: every record whose id belongs to one
// of its masters. Resolve maps a master name to its load-order index (0xFF if absent).
template <class Resolve>
inline bool ScanOverrides(Source& in, std::uint8_t self, Resolve&& resolve, std::vector<Override>& out) {
    std::vector<std::string> masters;
    if (!ReadMasters(in, masters)) return false;
    std::vector<std::uint8_t> index;
    for (auto& name : masters) index.push_back(resolve(name.c_str()));
    return WalkRecords(in, [&](const RecordHeader& h, Source&) {
        if ((h.formID >> 24) < masters.size()) {
            const std::uint32_t id = GlobalID(h.formID, index, self);
            if (id) out.push_back({id, self});
        }
        return false;
    });
}
// GetModItems: new (non-override) playable ARMO and WEAP records of one plugin, already
// turned into load-order ids. Compressed ARMO records cannot show their BODT flags
// without inflating them; the record flag still decides for those.
inline bool ScanItems(Source& in, std::uint8_t self, std::vector<std::uint32_t>& armors, std::vector<std::uint32_t>& weapons) {
    std::vector<std::string> masters;
    if (!ReadMasters(in, masters)) return false;
    return WalkRecords(in, [&](const RecordHeader& h, Source& src) {
        const bool armor = h.type == Tag("ARMO"), weapon = h.type == Tag("WEAP");
        if ((!armor && !weapon) || (h.formID >> 24) < masters.size()) return false;
        const std::uint32_t id = (std::uint32_t(self) << 24) | (h.formID & 0x00FFFFFF);
        bool playable = !(h.flags & kNonPlayableRecord);
        if (armor && playable && !(h.flags & kCompressed)) {
            std::vector<char> body(h.size);
            if (h.size && !src.Read(body.data(), h.size)) return false;
            std::size_t at = 0;
            while (at + 6 <= body.size()) {
                std::uint32_t type; std::uint16_t size;
                std::memcpy(&type, &body[at], 4); std::memcpy(&size, &body[at + 4], 2); at += 6;
                if (size > body.size() - at) break;
                if (type == Tag("BODT") && size >= 5 && (std::uint8_t(body[at + 4]) & 0x10)) playable = false;
                at += size;
            }
            if (playable) armors.push_back(id);
            return true;
        }
        if (playable) (armor ? armors : weapons).push_back(id);
        return false;
    });
}
// "getmoditem mod|vanilla|dlc": which loaded files a mode looks at (MfgConsole's lists).
constexpr const char* kDLCFiles[] = {"update.esm", "hearthfires.esm", "dawnguard.esm", "dragonborn.esm"};
inline bool ModeIncludes(int mode, const char* fileName) {
    bool isDLC = false;
    for (auto name : kDLCFiles) isDLC |= _stricmp(fileName, name) == 0;
    const bool vanilla = _stricmp(fileName, "skyrim.esm") == 0;
    return mode == 0 ? !vanilla && !isDLC : mode == 1 ? vanilla : mode == 2 ? isDLC : false;
}
}}
