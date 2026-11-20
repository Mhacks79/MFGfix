// 1.0.13 Mfg Console replacement: fakes and tests. Included by harness.cpp.
// The GFx layer is faked at the engine addresses the SKSE wrappers call (SetMember
// 0x00920D20, PushBack 0x00920EF0, GetArraySize 0x00920E80) and through a GFxMovieView
// vtable (CreateString +0x2C, CreateObject +0x34, CreateArray +0x38, CreateFunction +0x3C),
// so the real Legacy.cpp code builds real GFxValue trees that the tests then read back.
#include "../src/LegacyCore.h"
#include "../src/Legacy.h"
#include "../src/LegacyTables.h"
#include "skse/GameRTTI.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include <deque>
#include <map>
#include <string>
#include <vector>
namespace MfgLE { namespace Legacy { void TestOverrides(const std::vector<Override>& table); }}

namespace Fake {
struct Obj { std::map<std::string, GFxValue> members; std::vector<GFxValue> items; GFxFunctionHandler* fn = nullptr; };
std::deque<Obj> objects;
std::deque<std::string> strings;
UInt32 objectInterface[4];
void Make(GFxValue* v, UInt32 type) {
    objects.emplace_back();
    v->objectInterface = reinterpret_cast<GFxValue::ObjectInterface*>(objectInterface);
    v->type = type; v->data.obj = &objects.back();
}
Obj* Of(const GFxValue& v) { return static_cast<Obj*>(v.data.obj); }
void __fastcall CreateString(void*, void*, GFxValue* v, const char* s) {
    strings.emplace_back(s ? s : "");
    v->objectInterface = nullptr; v->type = GFxValue::kType_String; v->data.string = strings.back().c_str();
}
void __fastcall CreateObject(void*, void*, GFxValue* v, const char*, GFxValue*, UInt32) { Make(v, GFxValue::kType_Object); }
void __fastcall CreateArray(void*, void*, GFxValue* v) { Make(v, GFxValue::kType_Array); }
void __fastcall CreateFunction(void*, void*, GFxValue* v, GFxFunctionHandler* fn, void*) { Make(v, GFxValue::kType_Object); Of(*v)->fn = fn; }
bool __fastcall SetMember(void*, void*, void* obj, const char* name, GFxValue* value, bool) {
    static_cast<Obj*>(obj)->members[name] = *value; return true;
}
bool __fastcall PushBack(void*, void*, void* obj, GFxValue* value) { static_cast<Obj*>(obj)->items.push_back(*value); return true; }
UInt32 __fastcall ArraySize(void*, void*, void* obj) { return UInt32(static_cast<Obj*>(obj)->items.size()); }
void* viewTable[64];
void* view[2];
// Scaleform heap: Allocate (+0x28), Free (+0x30).
void* __fastcall HeapAllocate(void*, void*, UInt32 size, UInt32) { return malloc(size); }
void __fastcall HeapFree(void*, void*, void* p) { free(p); }
void* heapTable[16];
void* heap[1];
SKSEScaleformInterface::RegisterCallback scaleformCallback = nullptr;
const char* scaleformName = nullptr;
bool RegisterScaleform(const char* name, SKSEScaleformInterface::RegisterCallback cb) { scaleformName = name; scaleformCallback = cb; return true; }
SKSEScaleformInterface scaleform = {2, RegisterScaleform, nullptr};

// Forms. The base NPC carries TESFullName at +0x84 (name +0x88), as TESNPC does.
unsigned char npc[0x200], leveled[0x200], race[0x220], armor[0x140], noName[0x40];
std::map<const void*, UInt8> types;
void* __cdecl DynamicCast(void* src, UInt32, const void*, const void* to, UInt32) {
    FakeGame::NoteStackDepth();
    if (!src || !types.count(src)) return nullptr;
    const UInt8 t = types[src];
    if (to == RTTI_TESObjectREFR) return t == 0x3D || t == 0x3E ? src : nullptr;
    if (to == RTTI_Actor) return t == 0x3E ? src : nullptr;
    if (to == RTTI_TESFullName) return t == 0x2B ? static_cast<char*>(src) + 0x84 : nullptr;
    if (to == RTTI_TESNPC) return t == 0x2B ? src : nullptr;
    if (to == RTTI_TESRace) return t == 0x0E ? src : nullptr;
    if (to == RTTI_MagicItem || to == RTTI_SpellItem) return t == 0x16 ? src : nullptr;
    if (to == RTTI_EffectSetting) return t == 0x12 ? src : nullptr;
    return nullptr;
}
// Actor extra info: GetLevel 0x006A7320, MagicTarget (+0x54) slot 7, ActorValueOwner (+0x60)
// slots 1-3, MagicItem::GetCostliestEffectItem 0x00407860, hands in the process +0x68/+0x6C.
unsigned char spell[0x100], mgef[0xD0], activeEffect[0x70];
float effectItem[6];
struct Node { void* item; Node* next; } effectList = {activeEffect, nullptr};
UInt32 __fastcall FakeLevel(void*, void*) { return 27; }
void* __fastcall FakeCostliest(void*, void*, int, bool) { return effectItem; }
void* __fastcall FakeActiveEffects(void*, void*) { return &effectList; }
float __fastcall AVCurrent(void*, void*, UInt32 av) { return av + 0.5f; }
float __fastcall AVMaximum(void*, void*, UInt32 av) { return av + 1.5f; }
float __fastcall AVBase(void*, void*, UInt32 av) { return av + 2.5f; }
void* magicTargetTable[16];
void* avOwnerTable[8];
bool useLeveled = false;
TESForm* __cdecl LeveledBase(void*, UInt32, Actor* a) {
    return useLeveled ? reinterpret_cast<TESForm*>(leveled) : *reinterpret_cast<TESForm**>(reinterpret_cast<char*>(a) + 0x24);
}
unsigned resetCalls = 0; UInt32 resetArgs[5];
UInt32 __fastcall VanillaReset(void* self, void*, UInt32 a, UInt32 b, UInt32 c, UInt32 d, UInt32 e) {
    ++resetCalls; resetArgs[0]=a; resetArgs[1]=b; resetArgs[2]=c; resetArgs[3]=d; resetArgs[4]=e;
    FakeGame::Check(self == face, "vanilla reset still receives the face in ECX");
    return 0x55;
}
unsigned char dataHandler[0xAA0];
unsigned char mods[3][0x124];
DWORD clockNow = 1000;
DWORD WINAPI Clock() { return clockNow; }
UInt32 playerSlot[1];

void SetForm(unsigned char* f, UInt32 id, UInt8 type) {
    memset(f, 0, 0x20);
    *reinterpret_cast<UInt32*>(f + 0x0C) = id; f[0x12] = type; types[f] = type;
}
void Build() {
    for (auto& v : viewTable) v = nullptr;
    viewTable[0x2C / 4] = reinterpret_cast<void*>(CreateString);
    viewTable[0x34 / 4] = reinterpret_cast<void*>(CreateObject);
    viewTable[0x38 / 4] = reinterpret_cast<void*>(CreateArray);
    viewTable[0x3C / 4] = reinterpret_cast<void*>(CreateFunction);
    view[0] = viewTable;
    heapTable[10] = reinterpret_cast<void*>(HeapAllocate);
    heapTable[12] = reinterpret_cast<void*>(HeapFree);
    heap[0] = heapTable;
    FakeGame::Check(FakeGame::EnsurePage(0x01B40E3C, 4), "scaleform heap pointer mapped");
    FakeGame::WritePointer(0x01B40E3C, heap);
    FakeGame::WriteStub(0x00920D20, reinterpret_cast<void*>(SetMember));
    FakeGame::WriteStub(0x00920EF0, reinterpret_cast<void*>(PushBack));
    FakeGame::WriteStub(0x00920E80, reinterpret_cast<void*>(ArraySize));
    FakeGame::WriteStub(0x00F51DAE, reinterpret_cast<void*>(DynamicCast));
    FakeGame::WriteStub(0x00903570, reinterpret_cast<void*>(LeveledBase));
    // The three vanilla calls inside ModifyFaceGen.
    const UInt32 sites[][2] = {{MfgLE::Legacy::kPhonemeSite, MfgLE::Legacy::kPhonemeTarget},
                               {MfgLE::Legacy::kModifierSite, MfgLE::Legacy::kModifierTarget},
                               {MfgLE::Legacy::kResetSite, MfgLE::Legacy::kResetTarget}};
    for (auto& site : sites) {
        FakeGame::Check(FakeGame::EnsurePage(site[0], 5), "call site page mapped");
        auto code = reinterpret_cast<unsigned char*>(site[0]);
        code[0] = 0xE8; *reinterpret_cast<std::int32_t*>(code + 1) = std::int32_t(site[1] - (site[0] + 5));
    }
    // DataHandler: loadedModCount +0x69C, loadedMods +0x6A0, ModInfo name +0x20.
    const char* names[] = {"Skyrim.esm", "Dawnguard.esm", "My Mod.esp"};
    for (int i = 0; i < 3; ++i) {
        strcpy_s(reinterpret_cast<char*>(mods[i] + 0x20), 0x100, names[i]);
        *reinterpret_cast<void**>(dataHandler + 0x6A0 + i * 4) = mods[i];
    }
    *reinterpret_cast<UInt32*>(dataHandler + 0x69C) = 3;
    FakeGame::Check(FakeGame::EnsurePage(0x012E2CB8, 4), "data handler pointer mapped");
    FakeGame::WritePointer(0x012E2CB8, dataHandler);
    FakeGame::Check(FakeGame::EnsurePage(0x01B2E8E4, 4), "player pointer mapped");
    FakeGame::WritePointer(0x01B2E8E4, nullptr);
    MfgLE::Legacy::testClock = Clock;
    // Forms: base NPC "Lydia" 0x0100ABCD (Dawnguard), leveled base 0x0200ABCE (My Mod).
    SetForm(npc, 0x0100ABCD, 0x2B); SetForm(leveled, 0x0200ABCE, 0x2B); SetForm(race, 0x00013746, 0x0E);
    SetForm(armor, 0x00012E49, 0x1A); SetForm(noName, 0x00000ABC, 0x3D);
    *reinterpret_cast<const char**>(npc + 0x88) = "Lydia";
    *reinterpret_cast<const char**>(npc + 0x134) = "Lyd";
    *reinterpret_cast<float*>(npc + 0x12C) = 40.0f;
    *reinterpret_cast<UInt16*>(npc + 0x2C) = 12; *reinterpret_cast<UInt16*>(npc + 0x2E) = 1; *reinterpret_cast<UInt16*>(npc + 0x30) = 81;
    *reinterpret_cast<void**>(npc + 0xC4) = race;
    *reinterpret_cast<const char**>(race + 0x18) = "Nord";
    *reinterpret_cast<const char**>(race + 0x1D0) = "NordRace";
    race[0x80] = 10; race[0x81] = 5;   // first skill bonus
    *reinterpret_cast<const char**>(leveled + 0x88) = "Bandit";
    *reinterpret_cast<UInt32*>(armor + 0xE4) = 4; *reinterpret_cast<UInt32*>(armor + 0xE8) = 1;
    *reinterpret_cast<UInt32*>(armor + 0x110) = 2950;
    types[actorBytes] = 0x3E;
    SetForm(spell, 0x0001A4CC, 0x16); SetForm(mgef, 0x0001EA6F, 0x12);
    *reinterpret_cast<const char**>(spell + 0x24) = "Courage";
    *reinterpret_cast<const char**>(mgef + 0x18) = "Fear";
    effectItem[0] = 25.0f; reinterpret_cast<UInt32*>(effectItem)[1] = 0; reinterpret_cast<UInt32*>(effectItem)[2] = 30;
    reinterpret_cast<void**>(effectItem)[3] = mgef;
    *reinterpret_cast<void**>(activeEffect + 0x30) = spell;
    *reinterpret_cast<void**>(activeEffect + 0x34) = effectItem;
    *reinterpret_cast<float*>(activeEffect + 0x48) = 1.5f;
    *reinterpret_cast<float*>(activeEffect + 0x4C) = 30.0f;
    *reinterpret_cast<float*>(activeEffect + 0x50) = 20.0f;
    *reinterpret_cast<UInt32*>(activeEffect + 0x54) = 0x8000;
    FakeGame::WriteStub(0x006A7320, reinterpret_cast<void*>(FakeLevel));
    FakeGame::WriteStub(0x00407860, reinterpret_cast<void*>(FakeCostliest));
    magicTargetTable[7] = reinterpret_cast<void*>(FakeActiveEffects);
    avOwnerTable[1] = reinterpret_cast<void*>(AVCurrent);
    avOwnerTable[2] = reinterpret_cast<void*>(AVMaximum);
    avOwnerTable[3] = reinterpret_cast<void*>(AVBase);
}
// Called after the harness has zeroed and rebuilt the actor.
void ArmActor() {
    reinterpret_cast<TESForm*>(actorBytes)->formType = kFormType_Character;   // earlier tests change it
    *reinterpret_cast<void**>(actorBytes + 0x54) = magicTargetTable;
    *reinterpret_cast<void**>(actorBytes + 0x60) = avOwnerTable;
    process[0x68 / 4] = reinterpret_cast<UInt32>(armor);
}
TESForm* Find(UInt32 id) {
    if (id == 0x0100ABCD) return reinterpret_cast<TESForm*>(npc);
    if (id == 0x0200ABCE) return reinterpret_cast<TESForm*>(leveled);
    if (id == 0x00012E49) return reinterpret_cast<TESForm*>(armor);
    if (id == 0x00000ABC) return reinterpret_cast<TESForm*>(noName);
    return nullptr;
}
GFxFunctionHandler* Function(Obj* root, const char* name) {
    auto it = root->members.find(name);
    return it == root->members.end() ? nullptr : Of(it->second)->fn;
}
std::string Str(Obj* o, const char* name) {
    auto it = o->members.find(name);
    return it != o->members.end() && it->second.GetType() == GFxValue::kType_String ? it->second.GetString() : std::string("<missing>");
}
double Num(Obj* o, const char* name) {
    auto it = o->members.find(name);
    return it != o->members.end() && it->second.GetType() == GFxValue::kType_Number ? it->second.GetNumber() : -12345.0;
}
bool Has(Obj* o, const char* name) { return o->members.count(name) != 0; }
void Call(GFxFunctionHandler* fn, GFxValue* args, UInt32 count) {
    GFxValue result; GFxFunctionHandler::Args a = {&result, reinterpret_cast<GFxMovieView*>(view), nullptr, nullptr, args, count, nullptr};
    fn->Invoke(&a);
}
}  // namespace Fake

// A tiny plugin file: TES4 with masters, a GRUP, one override record, new ARMO/WEAP.
static void Put32(std::vector<unsigned char>& b, UInt32 v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); }
static void PutTag(std::vector<unsigned char>& b, const char* t) { b.insert(b.end(), t, t + 4); }
static void Sub(std::vector<unsigned char>& b, const char* t, const std::vector<unsigned char>& data) {
    PutTag(b, t); b.push_back(data.size() & 0xFF); b.push_back((data.size() >> 8) & 0xFF); b.insert(b.end(), data.begin(), data.end());
}
static void Record(std::vector<unsigned char>& b, const char* t, UInt32 flags, UInt32 id, const std::vector<unsigned char>& data) {
    PutTag(b, t); Put32(b, UInt32(data.size())); Put32(b, flags); Put32(b, id); Put32(b, 0); Put32(b, 0x002B0000);
    b.insert(b.end(), data.begin(), data.end());
}
static std::vector<unsigned char> Plugin(std::vector<std::string> masters) {
    std::vector<unsigned char> head, out;
    Sub(head, "HEDR", std::vector<unsigned char>(12, 0));
    for (auto& m : masters) { std::vector<unsigned char> name(m.begin(), m.end()); name.push_back(0); Sub(head, "MAST", name); Sub(head, "DATA", std::vector<unsigned char>(8, 0)); }
    Record(out, "TES4", 0, 0, head);
    std::vector<unsigned char> group;
    std::vector<unsigned char> bodtPlayable = {0x04, 0, 0, 0, 0x00, 0, 0, 0}, bodtHidden = {0x04, 0, 0, 0, 0x10, 0, 0, 0};
    std::vector<unsigned char> armorPlayable, armorHidden, armorForearm;
    Sub(armorPlayable, "EDID", {'a', 0}); Sub(armorPlayable, "BODT", bodtPlayable);
    Sub(armorHidden, "BODT", bodtHidden);
    Sub(armorForearm, "BODT", {0x10, 0, 0, 0, 0x00, 0, 0, 0});   // slot 34 only: playable
    Record(group, "ARMO", 0, 0x01000800, armorPlayable);           // new (index 1 = this file)
    Record(group, "ARMO", 0, 0x01000801, armorHidden);             // new, BODT non-playable
    Record(group, "ARMO", 0, 0x01000802, armorForearm);            // new, forearm slot
    Record(group, "ARMO", 0, 0x00012E49, armorPlayable);           // override of Skyrim.esm
    Record(group, "WEAP", 0, 0x01000900, {1, 2, 3});               // new weapon
    Record(group, "WEAP", 4, 0x01000901, {1, 2, 3});               // non-playable flag
    Record(group, "NPC_", 0, 0x0000ABCD, {9, 9});                  // override, master 0
    std::vector<unsigned char> grup;
    PutTag(grup, "GRUP"); Put32(grup, UInt32(group.size() + 24)); PutTag(grup, "ARMO"); Put32(grup, 0); Put32(grup, 0); Put32(grup, 0);
    grup.insert(grup.end(), group.begin(), group.end());
    out.insert(out.end(), grup.begin(), grup.end());
    return out;
}

static void LegacyCoreTests() {
    using FakeGame::Check;
    using namespace MfgLE::Legacy;
    char id[9]; StrID(0x0100ABCD, id); Check(!strcmp(id, "0100abcd"), "strFormID: 8 lower-case hex digits, zero padded");
    StrID(0x14, id); Check(!strcmp(id, "00000014"), "strFormID pads small ids");
    // 0.355f is 0.35499998..., so x87 (fld/fmul 100/fadd 0.5/_ftol2) gives 35: same here.
    Check(Percent(0.355f) == 35 && Percent(0.365f) == 37 && Percent(0.36f) == 36 && Percent(1.0f) == 100 && Percent(0.0f) == 0 && Percent(0.004f) == 0, "percent = value*100+0.5 truncated, float input exact");
    Check(Percent(-0.6f) == -59 && Percent(-0.5f) == -49 && Percent(-0.016f) == -1 && Percent(1e30f) == 0, "negatives truncate toward zero like _ftol2; out of range gives 0");
    float row[17] = {}; Check(FirstPositive(row, 17) == -1, "no expression when the row is all zero");
    row[3] = -1; row[9] = 0.4f; row[12] = 1; Check(FirstPositive(row, 17) == 9, "expression = first strictly positive entry");
    Check(TypeIndex(0x2B) == 0x2B && TypeIndex(0x200) == 0x8C, "type index capped at 0x8C");
    Check(!strcmp(kTypeNames[0x2B], "NPC(NPC_)") && !strcmp(kTypeNames[0x3E], "Character / PlayerCharacter(ACHR)") &&
          !strcmp(kTypeNames[0x8C], "unknown"), "type names table matches MfgConsole.dll");
    Check(!strcmp(kExpressionNames[0], " 0: Dialogue Anger") && !strcmp(kExpressionNames[16], "16: Combat Shout"), "expression names table");
    Check(BlinkStep(0, 7) == (7 % 50) * 4 + 100 && BlinkStep(10, 0) == 9, "blink counter re-arm and countdown");
    float v = -1;
    Check(!BlinkValue(5, v) && v == -1, "no blink write while the counter is at or above 5");
    Check(BlinkValue(4, v) && FakeGame::Near(v, 2.0f / 3, 1e-6f) && BlinkValue(3, v) && v == 1.0f &&
          BlinkValue(1, v) && FakeGame::Near(v, 1.0f / 3, 1e-6f) && BlinkValue(0, v) && v == 0, "blink triangle 2/3, 1, 1/3, 0");
    Check(ModeIncludes(0, "My Mod.esp") && !ModeIncludes(0, "Skyrim.esm") && !ModeIncludes(0, "Dawnguard.esm"), "getmoditem mod skips vanilla and DLC");
    Check(ModeIncludes(1, "skyrim.esm") && !ModeIncludes(1, "Update.esm"), "getmoditem vanilla = Skyrim.esm only");
    Check(ModeIncludes(2, "Dragonborn.esm") && ModeIncludes(2, "update.esm") && !ModeIncludes(2, "My Mod.esp"), "getmoditem dlc = the four DLC files");
    // Override scan with master mapping: this file's master 0 is Skyrim.esm (load index 0).
    auto plugin = Plugin({"Skyrim.esm"});
    MemorySource src(plugin.data(), plugin.size());
    std::vector<Override> found;
    auto resolve = [](const char* name) -> std::uint8_t { return !_stricmp(name, "Skyrim.esm") ? 0 : !_stricmp(name, "Dawnguard.esm") ? 1 : 0xFF; };
    Check(ScanOverrides(src, 2, resolve, found), "override scan walks the whole file");
    Check(found.size() == 2 && found[0].formID == 0x00012E49 && found[1].formID == 0x0000ABCD && found[0].file == 2,
          "override scan keeps only master records, with the file index");
    // The same plugin with Dawnguard as its only master: local 00 must become load index 01.
    auto dgPlugin = Plugin({"Dawnguard.esm"});
    MemorySource dg(dgPlugin.data(), dgPlugin.size());
    found.clear(); ScanOverrides(dg, 2, resolve, found);
    Check(found.size() == 2 && found[0].formID == 0x01012E49 && found[1].formID == 0x0100ABCD,
          "override ids mapped through the plugin's own MAST list");
    std::vector<std::uint32_t> armors, weapons;
    MemorySource items(plugin.data(), plugin.size());
    Check(ScanItems(items, 5, armors, weapons), "item scan walks the whole file");
    Check(armors.size() == 2 && armors[0] == 0x05000800 && armors[1] == 0x05000802, "new playable armour only; BODT flags 0x10 hides, forearm slot does not");
    Check(weapons.size() == 1 && weapons[0] == 0x05000900, "new playable weapon only; record flag 4 hides");
    std::vector<unsigned char> cut(plugin.begin(), plugin.begin() + plugin.size() - 3);
    MemorySource broken(cut.data(), cut.size());
    found.clear(); Check(!ScanOverrides(broken, 2, resolve, found), "truncated plugin reported, not over-read");
    unsigned char junk[8] = {'J', 'U', 'N', 'K'};
    MemorySource bad(junk, sizeof(junk));
    found.clear(); Check(!ScanOverrides(bad, 2, resolve, found) && found.empty(), "a file without TES4 is refused");
}

static void LegacyEngineTests() {
    using FakeGame::Check;
    using namespace MfgLE::Legacy;
    Check(Fake::scaleformName && !strcmp(Fake::scaleformName, "MfgConsole"), "registers the skse.plugins.MfgConsole namespace");
    Check(callSites == 7 && replacing == 1, "all three ModifyFaceGen call sites fixed without MfgConsole.dll");
    for (auto site : {kPhonemeSite, kModifierSite, kResetSite}) {
        auto code = reinterpret_cast<unsigned char*>(site);
        Check(code[0] == 0xE8, "call site still a call");
    }
    Check(*reinterpret_cast<std::int32_t*>(kPhonemeSite + 1) + kPhonemeSite + 5 == reinterpret_cast<UInt32>(&SetPhoneme), "phoneme call lands in the replacement");
    // A call that is not vanilla any more (MfgConsole got there first) is left alone.
    unsigned char foreign[5]; memcpy(foreign, reinterpret_cast<void*>(kModifierSite), 5);
    Check(!PatchCallSite(kModifierSite, kModifierTarget, reinterpret_cast<const void*>(&SetModifier)), "an already redirected call is never patched twice");
    Check(!memcmp(foreign, reinterpret_cast<void*>(kModifierSite), 5), "foreign call site bytes untouched");
    // The fixed setters, exactly as the old plugin: value in place, 0 -> 1e-7, isUpdated cleared.
    float half = 0.5f; UInt32 bits; memcpy(&bits, &half, 4);
    face->keyFrames[3].isUpdated = 1;
    SetPhoneme(face, nullptr, 5, bits);
    Check(phoneme[5] == 0.5f && face->keyFrames[3].isUpdated == 0, "mfg phoneme writes the value at once");
    SetPhoneme(face, nullptr, 5, 0);
    UInt32 tinyBits; memcpy(&tinyBits, &phoneme[5], 4);
    Check(tinyBits == 0x33D6BF95, "zero is written as 1e-7 (0x33D6BF95), as MfgConsole");
    const float before = phoneme[0];
    SetPhoneme(face, nullptr, 16, bits); SetPhoneme(face, nullptr, 0xFFFFFFFF, bits);
    Check(phoneme[0] == before, "phoneme id 16 and -1 rejected (the original only refused >= 16 signed)");
    SetModifier(face, nullptr, 16, bits);
    Check(modifier[16] == 0.5f && face->keyFrames[2].isUpdated == 0, "mfg modifier 16 accepted (17 modifiers)");
    SetModifier(face, nullptr, 17, bits);
    Check(modifier[16] == 0.5f, "modifier 17 rejected");
    // Stubbed only now: before load these bytes are a signature site the startup check reads.
    FakeGame::WriteStub(kResetTarget, reinterpret_cast<void*>(Fake::VanillaReset));
    face->unk1A3 = 0; face->unk1A4 = 1;
    using Reset = UInt32 (__fastcall*)(BSFaceGenAnimationData*, void*, UInt32, UInt32, UInt32, UInt32, UInt32);
    const UInt32 r = reinterpret_cast<Reset>(&ResetFace)(face, nullptr, 0x3F800000, 1, 1, 0, 0);
    bool clean = true;
    for (int i = 0; i < 16; ++i) clean &= phoneme[i] == 0;
    for (int i = 0; i < 17; ++i) clean &= modifier[i] == 0;
    Check(clean && face->unk1A3 == 1 && face->unk1A4 == 0, "mfg reset clears phonemes and modifiers and forces the copy edge");
    Check(Fake::resetCalls == 1 && r == 0x55 && Fake::resetArgs[0] == 0x3F800000 && Fake::resetArgs[1] == 1 && Fake::resetArgs[4] == 0,
          "then the vanilla reset runs with its five arguments and its result");
    // A console write cancels a smooth transition still running on that channel.
    MfgLE::Request request = {}; request.formID = 0x14; request.mode = 0; request.id = 6; request.value = 100; request.speed = 1;
    request.generation = MfgLE::generation;
    MfgLE::ApplyRequest(request);
    float quarter = 0.25f; memcpy(&bits, &quarter, 4);
    SetPhoneme(face, nullptr, 6, bits);
    Tick(0.5f);
    Check(phoneme[6] == 0.25f, "console value is not overwritten by an older smooth transition");
    // Plain verbs now take the engine path (live edit) without MfgConsole.dll.
    ConsoleInput console = {"phoneme", 4, 50}; UInt32 offset = 11; double result = 0;
    auto handler = reinterpret_cast<CommandInfo*>(0x0124D4D0)->execute;
    const int calls = originalCalls; const unsigned queuedBefore = queued;
    legacyConsoleOverride = 0;
    handler(nullptr, &console, actor, nullptr, nullptr, nullptr, &result, &offset);
    Check(originalCalls == calls + 1 && queued == queuedBefore, "plain verb goes to ModifyFaceGen (fixed call sites), not the queue");

    // Scaleform: register into a root object, then call the functions console.swf calls.
    GFxValue root; Fake::Make(&root, GFxValue::kType_Object);
    Check(Fake::scaleformCallback(reinterpret_cast<GFxMovieView*>(Fake::view), &root), "scaleform callback succeeds");
    Fake::Obj* api = Fake::Of(root);
    for (const char* name : {"GetObjectData", "GetObjectExtraData", "EnableExtraInfo", "PlayerBlinkSet", "GetModItems", "SaveBatFile", "ConsolePrint"})
        Check(Fake::Function(api, name) != nullptr, "console.swf function %s registered", name);
    SKSEMessagingInterface::Message loaded = {}; loaded.type = SKSEMessagingInterface::kMessage_DataLoaded;
    listener(&loaded);
    *reinterpret_cast<TESForm**>(actorBytes + 0x24) = reinterpret_cast<TESForm*>(Fake::npc);
    for (int i = 0; i < 16; ++i) phoneme[i] = 0;
    for (int i = 0; i < 17; ++i) { modifier[i] = 0; renderedExpression[i] = 0; }
    phoneme[2] = 0.36f; modifier[16] = 1.0f; renderedExpression[10] = 0.7f;
    MfgLE::Legacy::TestOverrides({{0x0100ABCD, 1}, {0x0100ABCD, 2}, {0x00000014, 2}});
    GFxValue args[2]; Fake::CreateString(nullptr, nullptr, &args[0], "14"); Fake::Make(&args[1], GFxValue::kType_Array);
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
    Fake::Obj* out = Fake::Of(args[1]);
    Check(out->items.size() == 2, "GetObjectData returns the info row and the face row for an actor");
    if (out->items.size() == 2) {
        Fake::Obj* info = Fake::Of(out->items[0]);
        Check(Fake::Str(info, "strType") == "NPC(NPC_)" && Fake::Num(info, "type") == 0x2B, "strType / type from the base form");
        Check(Fake::Str(info, "name") == "'Lydia'", "name quoted, from the base TESFullName");
        Check(Fake::Str(info, "strFormID") == "00000014" && Fake::Num(info, "formID") == 0x14, "formID");
        Check(Fake::Str(info, "strBaseID") == "0100abcd" && Fake::Num(info, "baseID") == double(0x0100ABCD), "baseID");
        Check(Fake::Str(info, "modName") == "Dawnguard.esm", "modName = the base's plugin");
        Check(Fake::Str(info, "locateModName") == "Skyrim.esm", "locateModName = the reference's plugin");
        auto base = info->members.find("baseOverwrite"), refr = info->members.find("refrOverwrite");
        Check(base != info->members.end() && Fake::Of(base->second)->items.size() == 2 &&
              std::string(Fake::Of(base->second)->items[1].GetString()) == "My Mod.esp", "baseOverwrite lists the overriding plugins, last = winner");
        Check(refr != info->members.end() && Fake::Of(refr->second)->items.size() == 1, "refrOverwrite");
        Fake::Obj* mfg = Fake::Of(out->items[1]);
        Check(Fake::Num(mfg, "phoneme2") == 36 && Fake::Num(mfg, "phoneme0") == 0 && Fake::Num(mfg, "modifier16") == 100,
              "phonemeN / modifierN in percent from the live face arrays");
        Check(Fake::Num(mfg, "expression") == 70 && Fake::Str(mfg, "expressionName") == "10: Mood Happy", "expression from keyframe 5");
        Check(Has(mfg, "phoneme15") && !Has(mfg, "phoneme16") && Has(mfg, "modifier16") && !Has(mfg, "modifier17"), "16 phonemes, 17 modifiers");
    }
    Fake::useLeveled = true;
    Fake::Make(&args[1], GFxValue::kType_Array);
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
    out = Fake::Of(args[1]);
    Check(!out->items.empty() && Fake::Str(Fake::Of(out->items[0]), "strBaseID") == "0200abce" &&
          Fake::Str(Fake::Of(out->items[0]), "modName") == "My Mod.esp" && Fake::Str(Fake::Of(out->items[0]), "name") == "'Lydia'",
          "baseID / modName follow the leveled base, name stays the placed base (as the original)");
    Fake::useLeveled = false;
    for (int i = 0; i < 17; ++i) renderedExpression[i] = 0;
    Fake::Make(&args[1], GFxValue::kType_Array);
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
    out = Fake::Of(args[1]);
    Check(out->items.size() == 2 && !Has(Fake::Of(out->items[1]), "expression"), "no expression entry when no expression is active");
    // Non-actor forms and bad input.
    Fake::CreateString(nullptr, nullptr, &args[0], "abc"); Fake::Make(&args[1], GFxValue::kType_Array);
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
    out = Fake::Of(args[1]);
    Check(out->items.size() == 1 && Fake::Str(Fake::Of(out->items[0]), "name") == "none" &&
          Fake::Str(Fake::Of(out->items[0]), "strType") == "unknown" && Fake::Str(Fake::Of(out->items[0]), "strBaseID") == "00000000",
          "reference without base: none / unknown / 00000000");
    for (const char* text : {"0", "ffffffff", "zz", "123456"}) {
        Fake::CreateString(nullptr, nullptr, &args[0], text); Fake::Make(&args[1], GFxValue::kType_Array);
        Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
        Check(Fake::Of(args[1])->items.empty(), "GetObjectData ignores id %s", text);
    }
    Fake::Make(&args[1], GFxValue::kType_Object);
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 2);
    Check(Fake::Of(args[1])->items.empty(), "GetObjectData refuses a non-array result");
    Fake::Call(Fake::Function(api, "GetObjectData"), args, 1);
    Check(true, "GetObjectData with one argument does not fault");
    // Extra info: off until enabled, then the base NPC with its race.
    InterlockedExchange(&extraInfo, 0);
    GFxValue extra[2]; extra[0].SetNumber(double(0x0100ABCD)); Fake::Make(&extra[1], GFxValue::kType_Object);
    Fake::Call(Fake::Function(api, "GetObjectExtraData"), extra, 2);
    Check(Fake::Of(extra[1])->members.empty(), "extra data answers nothing while extra info is off");
    InterlockedExchange(&extraInfo, 1);
    Fake::Call(Fake::Function(api, "GetObjectExtraData"), extra, 2);
    Fake::Obj* npcInfo = Fake::Of(extra[1]);
    Check(Fake::Num(npcInfo, "formType") == 0x2B && Fake::Num(npcInfo, "formId") == double(0x0100ABCD) &&
          Fake::Str(npcInfo, "modName") == "Dawnguard.esm" && Fake::Str(npcInfo, "name") == "Lydia", "extra data common fields");
    Check(Fake::Str(npcInfo, "fullName") == "Lydia" && Fake::Str(npcInfo, "shortName") == "Lyd" && Fake::Num(npcInfo, "weight") == 40 &&
          Fake::Num(npcInfo, "level") == 12 && Fake::Num(npcInfo, "minLevel") == 1 && Fake::Num(npcInfo, "maxLevel") == 81, "NPC fields");
    auto raceIt = npcInfo->members.find("race");
    Check(raceIt != npcInfo->members.end() && Fake::Str(Fake::Of(raceIt->second), "editorId") == "NordRace" &&
          Fake::Str(Fake::Of(raceIt->second), "fullName") == "Nord", "race child described recursively");
    if (raceIt != npcInfo->members.end()) {
        auto bonus = Fake::Of(raceIt->second)->members.find("skillBonuses");
        Check(bonus != Fake::Of(raceIt->second)->members.end() && Fake::Of(bonus->second)->items.size() == 7 &&
              Fake::Num(Fake::Of(Fake::Of(bonus->second)->items[0]), "skill") == 10 &&
              Fake::Num(Fake::Of(Fake::Of(bonus->second)->items[0]), "bonus") == 5, "race skill bonuses");
    }
    extra[0].SetNumber(double(0x00012E49)); Fake::Make(&extra[1], GFxValue::kType_Object);
    Fake::Call(Fake::Function(api, "GetObjectExtraData"), extra, 2);
    Check(Fake::Num(Fake::Of(extra[1]), "partMask") == 4 && Fake::Num(Fake::Of(extra[1]), "weightClass") == 1 &&
          Fake::Num(Fake::Of(extra[1]), "rating") == 29, "armour fields (rating / 100)");
    // Extra info of a placed actor: what the console's extra window shows for an NPC.
    Fake::ArmActor();
    extra[0].SetNumber(double(0x14)); Fake::Make(&extra[1], GFxValue::kType_Object);
    Fake::Call(Fake::Function(api, "GetObjectExtraData"), extra, 2);
    Fake::Obj* who = Fake::Of(extra[1]);
    Check(Fake::Num(who, "formType") == 0x3E && Fake::Num(who, "level") == 27, "actor: form type and GetLevel (0x006A7320)");
    auto baseIt = who->members.find("actorBase");
    Check(baseIt != who->members.end() && Fake::Str(Fake::Of(baseIt->second), "fullName") == "Lydia", "actor: actorBase described");
    auto fxIt = who->members.find("activeEffects");
    Check(fxIt != who->members.end() && Fake::Of(fxIt->second)->items.size() == 1, "actor: one active effect");
    if (fxIt != who->members.end() && Fake::Of(fxIt->second)->items.size() == 1) {
        Fake::Obj* fx = Fake::Of(Fake::Of(fxIt->second)->items[0]);
        Check(Fake::Num(fx, "elapsed") == 1.5 && Fake::Num(fx, "duration") == 30 && Fake::Num(fx, "magnitude") == 20, "active effect timings");
        auto inactive = fx->members.find("inactive");
        Check(inactive != fx->members.end() && inactive->second.GetBool(), "active effect flag 0x8000 = inactive");
        auto item = fx->members.find("magicItem"), effect = fx->members.find("magicEffect");
        Check(item != fx->members.end() && Fake::Str(Fake::Of(item->second), "spellName") == "Courage" &&
              Fake::Num(Fake::Of(item->second), "magnitude") == 25, "active effect magicItem with its costliest effect");
        Check(effect != fx->members.end() && Fake::Str(Fake::Of(effect->second), "effectName") == "Fear" &&
              Fake::Of(effect->second)->members.count("hideInUI"), "active effect magicEffect with hideInUI");
    }
    auto avIt = who->members.find("actorValues");
    Check(avIt != who->members.end() && Fake::Of(avIt->second)->items.size() == 0xA4, "actor: 164 actor values");
    if (avIt != who->members.end() && Fake::Of(avIt->second)->items.size() == 0xA4) {
        Fake::Obj* av = Fake::Of(Fake::Of(avIt->second)->items[5]);
        Check(Fake::Num(av, "current") == 5.5 && Fake::Num(av, "maximum") == 6.5 && Fake::Num(av, "base") == 7.5,
              "actor value current / maximum / base from ActorValueOwner slots 1-3");
    }
    auto eqIt = who->members.find("equipments");
    Check(eqIt != who->members.end() && Fake::Of(eqIt->second)->items.size() == 1 &&
          Fake::Num(Fake::Of(Fake::Of(eqIt->second)->items[0]), "partMask") == 4, "actor: right-hand item from the process");
    Check(Has(who, "spells") && !Has(who, "perkPoints"), "actor: spells array; perk points only for the player");
    // Player blink on the face update thread, driven by a fake 30 ms clock.
    FakeGame::WritePointer(0x01B2E8E4, actor);
    for (int i = 0; i < 17; ++i) renderedModifier[i] = 0;
    SetBlink(false); SetBlink(true);
    bool closed = false, partial = false;
    for (int i = 0; i < 400; ++i) {
        Fake::clockNow += 30; MfgLE::Legacy::AfterFaceUpdate(face);
        closed |= renderedModifier[0] == 1.0f && renderedModifier[1] == 1.0f;
        partial |= FakeGame::Near(renderedModifier[0], 2.0f / 3, 1e-5f) || FakeGame::Near(renderedModifier[0], 1.0f / 3, 1e-5f);
    }
    Check(closed && partial, "player blinks within 12 s of game time (full close and partial frames)");
    Check(renderedModifier[2] == 0, "only the two blink modifiers are written");
    unsigned char other[sizeof(BSFaceGenAnimationData)] = {};
    MfgLE::Legacy::AfterFaceUpdate(reinterpret_cast<BSFaceGenAnimationData*>(other));
    Check(true, "another actor's face update is ignored");
    renderedModifier[0] = renderedModifier[1] = 0.9f;
    Check(SetBlink(false), "blink stops");
    Fake::clockNow += 30; MfgLE::Legacy::AfterFaceUpdate(face);
    Check(renderedModifier[0] == 0 && renderedModifier[1] == 0, "stopping the blink opens the eyes");
    Fake::clockNow += 3000; MfgLE::Legacy::AfterFaceUpdate(face);
    Check(renderedModifier[0] == 0, "no blink after it is stopped");
    GFxValue on; on.SetBool(true);
    Fake::Call(Fake::Function(api, "PlayerBlinkSet"), &on, 1);
    Check(blinkEnabled == 1, "PlayerBlinkSet(true) from console.swf");
    on.SetBool(false); Fake::Call(Fake::Function(api, "PlayerBlinkSet"), &on, 1);
    Check(blinkEnabled == 0, "PlayerBlinkSet(false) from console.swf");
    FakeGame::WritePointer(0x01B2E8E4, nullptr);
    Check(MfgLE::Legacy::TestFaults() == 0 && !MfgLE::Legacy::disabled, "no fault in the Mfg Console layer");
}
