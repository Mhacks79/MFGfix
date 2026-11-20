// Mfg Console replacement (1.0.13). Every function below is MfgConsole.dll's (sha 25194ce2)
// own behaviour, rebuilt from its code; the address of the original routine is given in each
// comment. Offsets are the ones that DLL uses on TESV 1.9.32.0.8, every engine pointer is
// checked before it is followed (F2), and every Scaleform entry point is fault-guarded.
#include "Legacy.h"
#include "LegacyCore.h"
#include "LegacyTables.h"
#include "Engine.h"
#include "skse/GameAPI.h"
#include "skse/GameData.h"
#include "skse/GameForms.h"
#include "skse/GameObjects.h"
#include "skse/GameReferences.h"
#include "skse/GameRTTI.h"
#include "skse/GameExtraData.h"
#include "skse/GameBSExtraData.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformMovie.h"
#include "skse/Utilities.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace MfgLE { namespace Legacy {
volatile LONG replacing = 0, callSites = 0, blinkEnabled = 0, extraInfo = 0, disabled = 0;
namespace {
constexpr LONG kMaxFaults = 8;
constexpr int kMaxDepth = 6;
volatile LONG faults = 0;
CRITICAL_SECTION tableLock;            // guards modNames / overrides / scanState
volatile LONG tableLockReady = 0;
std::vector<std::string> modNames;    // load-order index -> file name (DataLoaded snapshot)
std::vector<Override> overrides;      // sorted (formID, file)
volatile LONG scanState = 0;          // 0 none, 1 running, 2 done
int blinkCounter = 0;                 // face update thread only
DWORD blinkClock = 0;               // face update thread only
volatile LONG blinkResetPending = 0;
BSFaceGenAnimationData* playerFace = nullptr;  // face update thread only
DWORD playerFaceAt = 0;                      // face update thread only

#ifdef MFGLE_TEST
}
DWORD (WINAPI* testClock)() = nullptr;   // the offline harness drives time
namespace {
DWORD Now() { return testClock ? testClock() : GetTickCount(); }
#else
DWORD Now() { return GetTickCount(); }
#endif
int LegacyFault(EXCEPTION_POINTERS* info) {
    const LONG count = InterlockedIncrement(&faults);
    Log("Mfg Console layer FAULT %ld: code %08X at %p", count, info->ExceptionRecord->ExceptionCode,
        info->ExceptionRecord->ExceptionAddress);
    if (count >= kMaxFaults) {
        InterlockedExchange(&disabled, 1);
        Log("Mfg Console layer disabled after %ld faults; face fixes unaffected", count);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
// Readable() wants 4-byte alignment; byte fields do not have it.
bool Mem(const void* p, size_t n) {
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    if (!at) return false;
    return Readable(reinterpret_cast<const void*>(at & ~std::uintptr_t(3)), n + (at & 3));
}
template <class T> bool Get(const void* base, unsigned offset, T& out) {
    if (!base) return false;
    auto p = static_cast<const char*>(base) + offset;
    if (!Mem(p, sizeof(T))) return false;
    memcpy(&out, p, sizeof(T));
    return true;
}
const void* Ptr(const void* base, unsigned offset) {
    const void* p = nullptr;
    return Get(base, offset, p) && Mem(p, 4) ? p : nullptr;
}
// A C string from the engine, only if all of it is mapped (bounded).
const char* Text(const char* s) {
    if (!s || !Mem(s, 1)) return nullptr;
    for (size_t n = 0; n < 4096; ++n) {
        if (((reinterpret_cast<std::uintptr_t>(s) + n) & 0xFFF) == 0 && !Mem(s + n, 1)) return nullptr;
        if (!s[n]) return s;
    }
    return nullptr;
}
const char* TextAt(const void* base, unsigned offset) {
    const char* s = nullptr;
    return Get(base, offset, s) ? Text(s) : nullptr;
}
UInt8 FormTypeOf(const TESForm* form) { UInt8 t = 0; Get(form, 0x12, t); return t; }  // TESForm::formType +0x12 (GetSavedFormType 0x00401A20)
UInt32 FormIDOf(const TESForm* form) { UInt32 id = 0; Get(form, 0x0C, id); return id; }
bool IsForm(const void* p) { return p && Mem(p, 0x18); }

void Number(GFxValue* obj, const char* name, double value) { GFxValue v; v.SetNumber(value); obj->SetMember(name, &v); }
void Bool(GFxValue* obj, const char* name, bool value) { GFxValue v; v.SetBool(value); obj->SetMember(name, &v); }
void String(GFxMovieView* view, GFxValue* obj, const char* name, const char* value) {
    GFxValue v; view->CreateString(&v, value ? value : ""); obj->SetMember(name, &v);
}

// Load-order file name for a mod index ("" when unknown), FUN_10003FB0. The original walks
// DataHandler+0x694 by position; SKSE's loadedMods array (+0x6A0, count +0x69C) is used
// when it looks sane, which is the same list for every normal load order.
std::string ModName(UInt32 index) {
    if (index >= 0xFF) return std::string();
    if (tableLockReady) {
        EnterCriticalSection(&tableLock);
        std::string name = index < modNames.size() ? modNames[index] : std::string();
        LeaveCriticalSection(&tableLock);
        if (!name.empty()) return name;
    }
    auto handler = reinterpret_cast<const char*>(Ptr(reinterpret_cast<void*>(0x012E2CB8), 0));
    if (!handler) return std::string();
    UInt32 count = 0;
    if (Get(handler, 0x69C, count) && count <= 0xFF && index < count) {
        auto file = reinterpret_cast<const char*>(Ptr(handler, 0x6A0 + index * 4));
        if (file) { const char* s = Text(file + 0x20); if (s) return s; }
    }
    // Fallback: the original's walk of tList<ModInfo> at +0x694.
    const void* node = handler + 0x694;
    for (UInt32 i = 0; node && i <= index; ++i) {
        const void* item = nullptr; Get(node, 0, item);
        if (!item) break;
        if (i == index) { const char* s = Text(static_cast<const char*>(item) + 0x20); return s ? s : std::string(); }
        node = Ptr(node, 4);
    }
    return std::string();
}

void Describe(GFxValue* out, GFxMovieView* view, TESForm* form, bool deep, int depth);
void Child(GFxValue* parent, GFxMovieView* view, const char* name, TESForm* form, bool deep, int depth) {
    GFxValue obj; view->CreateObject(&obj);
    Describe(&obj, view, form, deep, depth + 1);
    parent->SetMember(name, &obj);
}
void Push(GFxValue* array, GFxMovieView* view, TESForm* form, bool deep, int depth) {
    GFxValue obj; view->CreateObject(&obj);
    Describe(&obj, view, form, deep, depth + 1);
    array->PushBack(&obj);
}
template <class T> T* Cast(TESForm* form, const void* to) {
    return form ? static_cast<T*>(Runtime_DynamicCast(form, 0, RTTI_TESForm, to, 0)) : nullptr;
}
// thiscall getters MfgConsole calls through the component vtables.
const char* ModelOf(const void* model) {        // TESModel::GetModelName (vtable +0x10)
    if (!Mem(model, 8) || !Ptr(model, 0)) return nullptr;
    using Fn = const char* (__thiscall*)(const void*);
    auto fn = reinterpret_cast<Fn>(const_cast<void*>(Ptr(Ptr(model, 0), 0x10)));
    return fn ? Text(fn(model)) : nullptr;
}
const void* CallGetter(const void* component) {  // BGSEquipType::GetEquipSlot etc. (+0x10)
    if (!Mem(component, 8) || !Ptr(component, 0)) return nullptr;
    using Fn = const void* (__thiscall*)(const void*);
    auto fn = reinterpret_cast<Fn>(const_cast<void*>(Ptr(Ptr(component, 0), 0x10)));
    return fn ? fn(component) : nullptr;
}

// FUN_100040A0: formType, formId, modName, name.
void Common(GFxValue* out, GFxMovieView* view, TESForm* form) {
    const UInt32 id = FormIDOf(form);
    Number(out, "formType", FormTypeOf(form));
    Number(out, "formId", double(id));
    String(view, out, "modName", ModName(id >> 24).c_str());
    if (auto full = Cast<TESFullName>(form, RTTI_TESFullName)) {
        if (auto s = TextAt(full, 4)) String(view, out, "name", s);
    }
}
// FUN_100041F0: armour, book, container, weapon, ammo, potion, soul gem.
void Objects(GFxValue* out, GFxMovieView* view, TESForm* form) {
    const char* p = reinterpret_cast<const char*>(form);
    switch (FormTypeOf(form)) {
    case 0x1A: {  // ARMO
        UInt32 parts = 0, weight = 0, rating = 0;
        if (Get(p, 0xE4, parts)) Number(out, "partMask", parts);
        if (Get(p, 0xE8, weight)) Number(out, "weightClass", weight);
        if (Get(p, 0x110, rating)) Number(out, "rating", double(rating / 100));
        String(view, out, "maleGndModel", ModelOf(p + 0x64));
        String(view, out, "femaleGndModel", ModelOf(p + 0x80));
        UInt32 addons = 0;
        auto first = Ptr(Ptr(p, 0x114), 0);
        if (Get(p, 0x11C, addons) && addons && first) {
            String(view, out, "maleModel", ModelOf(static_cast<const char*>(first) + 0x34));
            String(view, out, "femaleModel", ModelOf(static_cast<const char*>(first) + 0x50));
        }
        break;
    }
    case 0x1B: {  // BOOK
        UInt8 flags = 0, type = 0;
        if (!Get(p, 0x94, flags)) break;
        Get(p, 0x95, type);
        Number(out, "flags", flags);
        Number(out, "bookType", type);
        if (flags & 4) {
            auto spell = Ptr(p, 0x98);
            Number(out, "teachesSpell", spell ? double(FormIDOf(static_cast<const TESForm*>(spell))) : -1.0);
        } else if (flags & 1) {
            UInt32 skill = 0; Get(p, 0x98, skill);
            Number(out, "teachesSkill", skill);
        }
        break;
    }
    case 0x1C: {  // CONT
        UInt32 flags = 0;
        if (Get(p, 0x64, flags)) Bool(out, "respawn", ((flags >> 9) & 1) != 0);
        break;
    }
    case 0x29: {  // WEAP
        UInt8 sub = 0; float speed = 0, reach = 0, stagger = 0, minRange = 0, maxRange = 0; UInt16 crit = 0;
        Get(p, 0xF1, sub); Get(p, 0xC4, speed); Get(p, 0xC8, reach); Get(p, 0xDC, stagger);
        Get(p, 0xFC, crit); Get(p, 0xCC, minRange); Get(p, 0xD0, maxRange);
        Number(out, "subType", sub); Number(out, "weaponType", sub);
        Number(out, "speed", speed); Number(out, "reach", reach); Number(out, "stagger", stagger);
        Number(out, "critDamage", crit); Number(out, "minRange", minRange); Number(out, "maxRange", maxRange);
        UInt16 damage = 0;
        if (Get(p, 0x68 + 4, damage)) Number(out, "baseDamage", damage);  // TESAttackDamageForm::attackDamage
        String(view, out, "model", ModelOf(p + 0x28));
        if (auto slot = CallGetter(p + 0x78)) if (IsForm(slot)) Number(out, "equipSlot", FormIDOf(static_cast<const TESForm*>(slot)));
        break;
    }
    case 0x2A: { UInt32 flags = 0; if (Get(p, 0x90, flags)) Number(out, "flags", flags); break; }   // AMMO
    case 0x2E: { UInt32 flags = 0; if (Get(p, 0xA8, flags)) Number(out, "flags", flags); break; }   // ALCH
    case 0x34: {  // SLGM
        UInt8 soul = 0, gem = 0;
        if (Get(p, 0x8C, soul) && Get(p, 0x8D, gem)) { Number(out, "soulSize", soul); Number(out, "gemSize", gem); }
        break;
    }
    default: break;
    }
}
// FUN_10004760: magic effects, spells / scrolls / ingredients / potions, shouts.
void Magic(GFxValue* out, GFxMovieView* view, TESForm* form, bool deep, int depth) {
    const char* p = reinterpret_cast<const char*>(form);
    switch (FormTypeOf(form)) {
    case 0x12: {  // MGEF
        auto effect = Cast<EffectSetting>(form, RTTI_EffectSetting);
        if (!effect) break;
        const char* e = reinterpret_cast<const char*>(effect);
        if (auto s = TextAt(e, 0x18)) String(view, out, "effectName", s);
        static_assert(sizeof(UInt32) == 4, "");
        const struct { const char* name; unsigned offset; } ints[] = {
            {"subType", 0x44}, {"effectFlags", 0x38}, {"school", 0x44}, {"skillLevel", 0x60},
            {"archetype", 0x78}, {"deliveryType", 0x8C}};
        for (auto& f : ints) { UInt32 v = 0; if (Get(e, f.offset, v)) Number(out, f.name, v); }
        float cast = 0, delay = 0;
        if (Get(e, 0x68, cast)) Number(out, "castTime", cast);
        if (Get(e, 0xCC, delay)) Number(out, "delayTime", delay);
        const struct { const char* name; unsigned offset; } more[] = {{"actorValue", 0x7C}, {"castType", 0x88}, {"magicType", 0x48}};
        for (auto& f : more) { UInt32 v = 0; if (Get(e, f.offset, v)) Number(out, f.name, v); }
        break;
    }
    case 0x16: case 0x17: case 0x1E: case 0x2E: {  // SPEL SCRL INGR ALCH
        auto item = Cast<MagicItem>(form, RTTI_MagicItem);
        if (!item) break;
        if (auto s = TextAt(item, 0x24)) String(view, out, "spellName", s);
        auto costliest = CALL_MEMBER_FN(item, GetCostliestEffectItem)(5, false);
        if (Mem(costliest, 0x10) && IsForm(costliest->mgef)) {
            Number(out, "magnitude", costliest->magnitude);
            Number(out, "duration", costliest->duration);
            Number(out, "area", costliest->area);
            Child(out, view, "magicEffect", costliest->mgef, deep, depth);
        }
        if (auto spell = Cast<SpellItem>(item, RTTI_SpellItem)) {
            UInt32 type = 0, cost = 0;
            if (Get(spell, 0x74, type)) Number(out, "spellType", type);
            if (Get(spell, 0x6C, cost)) Number(out, "trueCost", cost);
            if (auto slot = CallGetter(reinterpret_cast<const char*>(spell) + 0x50))
                if (IsForm(slot)) Number(out, "equipSlot", FormIDOf(static_cast<const TESForm*>(slot)));
        }
        if (auto potion = Cast<AlchemyItem>(item, RTTI_AlchemyItem)) {
            auto sound = static_cast<TESForm*>(const_cast<void*>(Ptr(potion, 0xB4)));
            if (IsForm(sound)) Child(out, view, "useSound", sound, deep, depth);
        }
        break;
    }
    case 0x77: {  // SHOU
        auto shout = Cast<TESShout>(form, RTTI_TESShout);
        if (!shout) break;
        const char* s = reinterpret_cast<const char*>(shout);
        if (auto name = TextAt(s, 0x18)) String(view, out, "fullName", name);
        GFxValue words; view->CreateArray(&words);
        for (unsigned i = 0; i < 3; ++i) {
            const char* entry = s + 0x38 + i * 0xC;   // {TESWordOfPower*, SpellItem*, float}
            GFxValue obj; view->CreateObject(&obj);
            if (auto word = Ptr(entry, 0)) {
                if (auto t = TextAt(word, 0x18)) String(view, &obj, "word", t);
                if (auto t = TextAt(word, 0x1C)) String(view, &obj, "fullName", t);
            }
            float recovery = 0; Get(entry, 8, recovery);
            Number(&obj, "recoveryTime", recovery);
            auto spell = static_cast<TESForm*>(const_cast<void*>(Ptr(entry, 4)));
            if (IsForm(spell)) Describe(&obj, view, spell, deep, depth + 1);
            words.PushBack(&obj);
        }
        out->SetMember("words", &words);
        break;
    }
    default: break;
    }
}
// TESSpellList::Data: spells @0, shouts @8, counts @0xC / @0x14.
void SpellList(GFxValue* out, GFxMovieView* view, const void* data, bool deep, int depth) {
    GFxValue spells, shouts; view->CreateArray(&spells); view->CreateArray(&shouts);
    UInt32 nSpells = 0, nShouts = 0;
    if (data && Get(data, 0xC, nSpells) && nSpells < 4096)
        for (UInt32 i = 0; i < nSpells; ++i) {
            auto f = static_cast<TESForm*>(const_cast<void*>(Ptr(Ptr(data, 0), i * 4)));
            if (IsForm(f)) Push(&spells, view, f, deep, depth);
        }
    if (data && Get(data, 0x14, nShouts) && nShouts < 4096)
        for (UInt32 i = 0; i < nShouts; ++i) {
            auto f = static_cast<TESForm*>(const_cast<void*>(Ptr(Ptr(data, 8), i * 4)));
            if (IsForm(f)) Push(&shouts, view, f, deep, depth);
        }
    out->SetMember("spells", &spells);
    out->SetMember("shouts", &shouts);
}
class MatchBySlot : public FormMatcher {
    UInt32 mask;
public:
    explicit MatchBySlot(UInt32 slot) : mask(slot) {}
    bool Matches(TESForm* form) const override {
        auto biped = form ? DYNAMIC_CAST(form, TESForm, BGSBipedObjectForm) : nullptr;
        return biped && (biped->data.parts & mask) != 0;
    }
};
// FUN_10004DD0: race, actor base, actor.
void Actors(GFxValue* out, GFxMovieView* view, TESForm* form, bool deep, int depth) {
    const char* p = reinterpret_cast<const char*>(form);
    switch (FormTypeOf(form)) {
    case 0x0E: {  // RACE
        if (!Cast<TESRace>(form, RTTI_TESRace)) break;
        if (auto s = TextAt(p, 0x18)) String(view, out, "fullName", s);
        if (auto s = TextAt(p, 0x1D0)) String(view, out, "editorId", s);
        SpellList(out, view, Ptr(p, 0x2C), deep, depth);
        GFxValue bonuses; view->CreateArray(&bonuses);
        for (unsigned i = 0; i < 7; ++i) {
            UInt8 skill = 0, bonus = 0;
            Get(p, 0x80 + i * 2, skill); Get(p, 0x81 + i * 2, bonus);
            GFxValue obj; view->CreateObject(&obj);
            Number(&obj, "skill", skill); Number(&obj, "bonus", bonus);
            bonuses.PushBack(&obj);
        }
        out->SetMember("skillBonuses", &bonuses);
        break;
    }
    case 0x2B: {  // NPC_
        if (!Cast<TESNPC>(form, RTTI_TESNPC)) break;
        if (auto s = TextAt(p, 0x88)) String(view, out, "fullName", s);
        if (auto s = TextAt(p, 0x134)) String(view, out, "shortName", s);
        float weight = 0; if (Get(p, 0x12C, weight)) Number(out, "weight", weight);
        auto race = static_cast<TESForm*>(const_cast<void*>(Ptr(p, 0xC4)));
        if (IsForm(race)) Child(out, view, "race", race, deep, depth);
        SpellList(out, view, Ptr(p, 0x68), deep, depth);
        UInt16 level = 0, minLevel = 0, maxLevel = 0;
        Get(p, 0x2C, level); Get(p, 0x2E, minLevel); Get(p, 0x30, maxLevel);
        Number(out, "level", level); Number(out, "minLevel", minLevel); Number(out, "maxLevel", maxLevel);
        break;
    }
    case 0x3E: {  // ACHR
        auto actor = Cast<Actor>(form, RTTI_Actor);
        if (!actor) break;
        Number(out, "level", CALL_MEMBER_FN(actor, GetLevel)());
        auto base = static_cast<TESForm*>(const_cast<void*>(Ptr(p, 0x24)));
        if (IsForm(base)) Child(out, view, "actorBase", base, deep, depth);
        // Added spells: count +0x104, storage +0x100 (inline single entry when bit 31 of +0xFC).
        GFxValue spells; view->CreateArray(&spells);
        UInt32 count = 0, capacity = 0;
        if (Get(p, 0x104, count) && Get(p, 0xFC, capacity) && count < 4096)
            for (UInt32 i = 0; i < count; ++i) {
                const void* spell = nullptr;
                if (capacity & 0x80000000u) { if (i == 0) Get(p, 0x100, spell); }
                else spell = Ptr(Ptr(p, 0x100), i * 4);
                GFxValue obj; view->CreateObject(&obj);
                if (IsForm(spell)) Describe(&obj, view, static_cast<TESForm*>(const_cast<void*>(spell)), deep, depth + 1);
                spells.PushBack(&obj);
            }
        out->SetMember("spells", &spells);
        // Active effects through MagicTarget (+0x54) -> GetActiveEffects (vtable +0x1C).
        GFxValue effects; view->CreateArray(&effects);
        tList<ActiveEffect>* list = Mem(p + 0x54, 4) && Ptr(p, 0x54) ? actor->magicTarget.GetActiveEffects() : nullptr;
        if (Mem(list, 8)) {
            const UInt32 n = list->Count();
            for (UInt32 i = 0; i < n && i < 4096; ++i) {
                ActiveEffect* fx = list->GetNthItem(i);
                if (!Mem(fx, 0x58)) continue;
                GFxValue obj; view->CreateObject(&obj);
                if (IsForm(fx->item)) Child(&obj, view, "magicItem", fx->item, deep, depth);
                Number(&obj, "elapsed", fx->elapsed);
                Number(&obj, "duration", fx->duration);
                Number(&obj, "magnitude", fx->magnitude);
                Bool(&obj, "inactive", (fx->flags & 0x8000) == 0x8000);
                auto mgef = Ptr(fx->effect, 0xC);
                if (Mem(fx->effect, 0x10) && IsForm(mgef)) {
                    GFxValue child; view->CreateObject(&child);
                    Describe(&child, view, static_cast<TESForm*>(const_cast<void*>(mgef)), deep, depth + 1);
                    UInt32 flags = 0; Get(mgef, 0x38, flags);
                    Bool(&child, "hideInUI", (flags & 0x8000) == 0x8000);
                    obj.SetMember("magicEffect", &child);
                }
                effects.PushBack(&obj);
            }
        }
        out->SetMember("activeEffects", &effects);
        // Actor values 0..0xA3 through ActorValueOwner (+0x60).
        GFxValue values; view->CreateArray(&values);
        for (UInt32 av = 0; av < 0xA4; ++av) {
            GFxValue obj; view->CreateObject(&obj);
            Number(&obj, "current", actor->actorValueOwner.GetCurrent(av));
            Number(&obj, "maximum", actor->actorValueOwner.GetMaximum(av));
            Number(&obj, "base", actor->actorValueOwner.GetBase(av));
            values.PushBack(&obj);
        }
        out->SetMember("actorValues", &values);
        // Equipment: both hands from the process (+0x88 -> +0x6C, +0x68), then worn armour by slot.
        GFxValue equipment; view->CreateArray(&equipment);
        if (auto process = Ptr(p, 0x88)) {
            for (unsigned offset : {0x6Cu, 0x68u}) {
                auto item = static_cast<TESForm*>(const_cast<void*>(Ptr(process, offset)));
                if (IsForm(item)) Push(&equipment, view, item, deep, depth);
            }
        }
        auto changes = static_cast<ExtraContainerChanges*>(actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (Mem(changes, 0xC)) {
            TESForm* seen[32] = {};
            for (unsigned slot = 0; slot < 32; ++slot) {
                MatchBySlot matcher(1u << slot);
                TESForm* worn = changes->FindEquipped(matcher).pForm;
                if (!IsForm(worn)) continue;
                for (unsigned j = 0; j < 32; ++j) {
                    if (seen[j] == worn) break;
                    if (!seen[j]) { seen[j] = worn; Push(&equipment, view, worn, deep, depth); break; }
                }
            }
        }
        out->SetMember("equipments", &equipment);
        if (auto player = Cast<PlayerCharacter>(form, RTTI_PlayerCharacter)) {
            UInt8 points = 0;
            if (Get(player, 0x6E1, points)) Number(out, "perkPoints", points);
        }
        break;
    }
    default: break;
    }
}
// FUN_10006010: texture set, quest, message.
void Misc(GFxValue* out, GFxMovieView* view, TESForm* form) {
    const char* p = reinterpret_cast<const char*>(form);
    switch (FormTypeOf(form)) {
    case 0x07: {  // TXST
        if (!Cast<BGSTextureSet>(form, RTTI_BGSTextureSet)) break;
        GFxValue textures; view->CreateArray(&textures);
        for (unsigned i = 0; i < 8; ++i) {
            GFxValue s; view->CreateString(&s, TextAt(p, 0x2C + i * 8) ? TextAt(p, 0x2C + i * 8) : "");
            textures.PushBack(&s);
        }
        out->SetMember("textures", &textures);
        break;
    }
    case 0x4D: {  // QUST
        if (!Cast<TESQuest>(form, RTTI_TESQuest)) break;
        String(view, out, "fullName", TextAt(p, 0x1C));
        UInt16 flags = 0; UInt8 priority = 0;
        Get(p, 0x80, flags); Get(p, 0x82, priority);
        Number(out, "flags", flags); Number(out, "priority", priority);
        const char* editor = TextAt(p, 0x13C);
        String(view, out, "editorId", editor ? editor : "");
        GFxValue aliases; view->CreateArray(&aliases);
        UInt32 count = 0;
        if (Get(p, 0x38, count) && count < 4096)
            for (UInt32 i = 0; i < count; ++i) {
                auto alias = Ptr(Ptr(p, 0x30), i * 4);
                if (!alias) continue;
                GFxValue obj; view->CreateObject(&obj);
                String(view, &obj, "name", TextAt(alias, 4));
                UInt32 id = 0, aflags = 0; Get(alias, 0xC, id); Get(alias, 0x10, aflags);
                Number(&obj, "id", id); Number(&obj, "flags", aflags);
                aliases.PushBack(&obj);
            }
        out->SetMember("aliases", &aliases);
        break;
    }
    case 0x69: {  // MESG
        if (!Cast<BGSMessage>(form, RTTI_BGSMessage)) break;
        GFxValue buttons; view->CreateArray(&buttons);
        const void* node = p + 0x30;          // tList of buttons, text pointer first
        for (unsigned i = 0; node && i < 64; ++i) {
            auto item = Ptr(node, 0);
            if (!item) break;
            GFxValue s; view->CreateString(&s, TextAt(item, 0) ? TextAt(item, 0) : "");
            buttons.PushBack(&s);
            node = Ptr(node, 4);
        }
        out->SetMember("buttons", &buttons);
        BSString text;                         // TESDescription (+0x1C) ::Get(out, form, 'DESC')
        auto description = reinterpret_cast<TESDescription*>(const_cast<char*>(p) + 0x1C);
        CALL_MEMBER_FN(description, Get)(&text, form, 'DESC');
        String(view, out, "message", Text(text.Get()));
        break;
    }
    default: break;
    }
}
// FUN_10005DB0: form lists (both the ESP entries and the ones added at run time).
void Lists(GFxValue* out, GFxMovieView* view, TESForm* form, bool deep, int depth) {
    if (FormTypeOf(form) != 0x5B || !Cast<BGSListForm>(form, RTTI_BGSListForm)) return;
    const char* p = reinterpret_cast<const char*>(form);
    GFxValue forms; view->CreateArray(&forms);
    UInt32 count = 0;
    if (Get(p, 0x1C, count) && count < 65536)
        for (UInt32 i = 0; i < count; ++i) {
            auto f = static_cast<TESForm*>(const_cast<void*>(Ptr(Ptr(p, 0x14), i * 4)));
            GFxValue obj; view->CreateObject(&obj);
            if (IsForm(f)) Describe(&obj, view, f, deep, depth + 1);
            forms.PushBack(&obj);
        }
    if (auto added = Ptr(p, 0x20)) {
        UInt32 n = 0;
        if (Get(added, 8, n) && n < 65536)
            for (UInt32 i = 0; i < n; ++i) {
                UInt32 id = 0;
                if (!Get(Ptr(added, 0), i * 4, id)) continue;
                TESForm* f = LookupFormByID(id);
                if (!IsForm(f)) continue;
                GFxValue obj; view->CreateObject(&obj);
                Describe(&obj, view, f, deep, depth + 1);
                forms.PushBack(&obj);
            }
    }
    out->SetMember("forms", &forms);
}
// FUN_100064D0.
void Describe(GFxValue* out, GFxMovieView* view, TESForm* form, bool deep, int depth) {
    if (!IsForm(form) || !out->IsObject() || depth > kMaxDepth) return;
    Common(out, view, form);
    if (deep) {
        Objects(out, view, form);
        Magic(out, view, form, deep, depth);
        Actors(out, view, form, deep, depth);
        Misc(out, view, form);
    }
    Lists(out, view, form, deep, depth);
}

// Overwrite table (FUN_100035B0), built off the main thread from the DataLoaded snapshot.
DWORD WINAPI ScanThread(void*) {
    std::vector<std::string> names;
    EnterCriticalSection(&tableLock); names = modNames; LeaveCriticalSection(&tableLock);
    std::vector<Override> found;
    auto resolve = [&](const char* master) -> std::uint8_t {
        for (size_t i = 0; i < names.size() && i < 0xFF; ++i) if (!_stricmp(names[i].c_str(), master)) return std::uint8_t(i);
        return 0xFF;
    };
    for (size_t i = 0; i < names.size() && i < 0xFF; ++i) {
        std::string path = "data\\" + names[i];
        FileSource file(path.c_str());
        if (!file.Open()) { Log("overwrite scan: %s not found", path.c_str()); continue; }
        if (!ScanOverrides(file, std::uint8_t(i), resolve, found)) Log("overwrite scan: %s ended early", path.c_str());
    }
    std::sort(found.begin(), found.end());
    EnterCriticalSection(&tableLock);
    overrides.swap(found);
    LeaveCriticalSection(&tableLock);
    InterlockedExchange(&scanState, 2);
    Log("overwrite scan done: %u plugins, %u override records", unsigned(names.size()), unsigned(overrides.size()));
    return 0;
}
void StartScan() {
    if (InterlockedCompareExchange(&scanState, 1, 0) != 0) return;
    EnterCriticalSection(&tableLock);
    const bool ready = !modNames.empty();
    LeaveCriticalSection(&tableLock);
    if (!ready) { InterlockedExchange(&scanState, 0); return; }
    HANDLE thread = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread); else InterlockedExchange(&scanState, 0);
}
void OverwriteNames(UInt32 formID, GFxValue* array, GFxMovieView* view) {
    if (scanState != 2 || !formID) return;
    EnterCriticalSection(&tableLock);
    auto it = std::lower_bound(overrides.begin(), overrides.end(), Override{formID, 0});
    for (; it != overrides.end() && it->formID == formID; ++it) {
        GFxValue s; view->CreateString(&s, it->file < modNames.size() ? modNames[it->file].c_str() : "");
        array->PushBack(&s);
    }
    LeaveCriticalSection(&tableLock);
}

// ---------------------------------------------------------------------------------------
// Scaleform functions. Each Invoke is a fault guard around a body that owns C++ objects.
const char* StringArg(GFxFunctionHandler::Args* a, UInt32 i) {
    return a->numArgs > i && a->args[i].GetType() == GFxValue::kType_String ? a->args[i].GetString() : nullptr;
}
bool TypeIs(GFxFunctionHandler::Args* a, UInt32 i, UInt32 type) { return a->numArgs > i && a->args[i].GetType() == type; }

// 0x10006620 GetObjectData(id:String, result:Array)
void GetObjectDataBody(GFxFunctionHandler::Args* a) {
    const char* text = StringArg(a, 0);
    if (!text || !TypeIs(a, 1, GFxValue::kType_Array)) {
        Log("    ERROR: system: invalid arguments. MfgConsole.GetObjectData(id:String, result:Array).");
        return;
    }
    const unsigned long id = strtoul(text, nullptr, 16);
    if (!id || id == 0xFFFFFFFFul) return;
    TESForm* form = LookupFormByID(id);
    if (!IsForm(form)) return;
    GFxMovieView* view = a->movie;
    GFxValue* result = &a->args[1];
    std::string name = "none";
    UInt32 type = 0x8C, baseID = 0;
    auto ref = Cast<TESObjectREFR>(form, RTTI_TESObjectREFR);
    if (auto base = ref ? static_cast<TESForm*>(const_cast<void*>(Ptr(ref, 0x24))) : nullptr) {
        if (IsForm(base)) {
            baseID = FormIDOf(base);
            type = FormTypeOf(base);
            if (auto full = Cast<TESFullName>(base, RTTI_TESFullName))
                if (auto s = TextAt(full, 4)) name = std::string("'") + s + "'";
        }
    }
    Actor* actor = Cast<Actor>(form, RTTI_Actor);
    if (actor) {  // the leveled actor base, through the Papyrus native at 0x00903570
        using Fn = TESForm* (__cdecl*)(void*, UInt32, Actor*);
        TESForm* leveled = reinterpret_cast<Fn>(0x00903570)(nullptr, 0, actor);
        if (IsForm(leveled)) baseID = FormIDOf(leveled);
    }
    UInt32 modIndex = baseID ? baseID >> 24 : 0xFF;
    if (modIndex == 0xFF) modIndex = id >> 24;
    const std::string modName = ModName(modIndex);
    std::string locate = ModName(id >> 24);
    if (locate.empty()) locate = "unknown";
    char strForm[9], strBase[9];
    StrID(id, strForm); StrID(baseID, strBase);
    GFxValue info; view->CreateObject(&info);
    String(view, &info, "strType", kTypeNames[TypeIndex(type)]);
    String(view, &info, "name", name.c_str());
    String(view, &info, "strFormID", strForm);
    String(view, &info, "strBaseID", strBase);
    String(view, &info, "modName", modName.c_str());
    String(view, &info, "locateModName", locate.c_str());
    Number(&info, "type", TypeIndex(type));
    Number(&info, "formID", double(id));
    Number(&info, "baseID", double(baseID));
    if (scanState == 2) {
        GFxValue refr, base; view->CreateArray(&refr); view->CreateArray(&base);
        OverwriteNames(id, &refr, view);
        OverwriteNames(baseID, &base, view);
        if (refr.GetArraySize()) info.SetMember("refrOverwrite", &refr);
        if (base.GetArraySize()) info.SetMember("baseOverwrite", &base);
    }
    result->PushBack(&info);
    if (!actor) return;
    BSFaceGenAnimationData* face = FaceOf(actor);
    if (!face) return;
    float phoneme[16], modifier[17], expression[17];
    bool expressionOK = false;
    if (!CopyFace(face, phoneme, modifier, expression, expressionOK)) return;
    GFxValue mfg; view->CreateObject(&mfg);
    char key[24];
    for (int i = 0; i < 16; ++i) { sprintf_s(key, "phoneme%d", i); Number(&mfg, key, Percent(phoneme[i])); }
    for (int i = 0; i < 17; ++i) { sprintf_s(key, "modifier%d", i); Number(&mfg, key, Percent(modifier[i])); }
    const int shown = expressionOK ? FirstPositive(expression, 17) : -1;
    if (shown >= 0) {
        Number(&mfg, "expression", Percent(expression[shown]));
        String(view, &mfg, "expressionName", kExpressionNames[shown]);
    }
    result->PushBack(&mfg);
}
// 0x10006570 GetObjectExtraData(formID:Number, result:Object)
void GetObjectExtraDataBody(GFxFunctionHandler::Args* a) {
    if (!TypeIs(a, 0, GFxValue::kType_Number) || !TypeIs(a, 1, GFxValue::kType_Object)) {
        Log("    ERROR: system: invalid arguments. MfgConsole.GetObjectExtraData(formID:Number, result:Object).");
        return;
    }
    const double number = a->args[0].GetNumber();
    if (!extraInfo || !(number >= 1 && number <= 4294967295.0)) return;
    TESForm* form = LookupFormByID(static_cast<UInt32>(number + 0.5));
    if (IsForm(form)) Describe(&a->args[1], a->movie, form, true, 0);
}
// 0x10007380 EnableExtraInfo()
void EnableExtraInfoBody(GFxFunctionHandler::Args*) {
    InterlockedExchange(&extraInfo, 1);
    StartScan();
}
// 0x10007D10 PlayerBlinkSet(inEnable:Boolean)
void PlayerBlinkSetBody(GFxFunctionHandler::Args* a) {
    if (!TypeIs(a, 0, GFxValue::kType_Bool)) {
        Log("    ERROR: system: invalid arguments. MfgConsole.PlayerBlinkSet(inEnable:Boolean).");
        return;
    }
    const bool on = a->args[0].GetBool();
    if (SetBlink(on)) Console_Print(on ? " playerBlink -> ON." : " playerBlink -> OFF.");
}
// 0x10008080 ConsolePrint(str:String)
void ConsolePrintBody(GFxFunctionHandler::Args* a) {
    const char* text = StringArg(a, 0);
    if (!text) { Log("    ERROR: system: invalid arguments. MfgConsole.ConsolePrint(str:string)."); return; }
    Log("%s", text);
    Console_Print("%s", text);
}
// 0x10007D80 SaveBatFile(fileName:String, saveStr:String) -> <game folder>\<fileName>.txt
void SaveBatFileBody(GFxFunctionHandler::Args* a) {
    const char* file = StringArg(a, 0); const char* text = StringArg(a, 1);
    if (!file || !text) {
        Log("    ERROR: system: invalid arguments. MfgConsole.SaveBatFile(fileName:string, saveStr:String).");
        return;
    }
    const std::string path = std::string(file) + ".txt";
    FILE* out = nullptr;
    if (!fopen_s(&out, path.c_str(), "wb") && out) {
        const size_t length = strlen(text);
        const bool ok = fwrite(text, 1, length, out) == length;
        fclose(out);
        if (ok) { Log("%s saved.", path.c_str()); Console_Print("- %s saved.", path.c_str()); return; }
    }
    Log("    ERROR: cannot write new file (%s).", path.c_str());
    Console_Print("- cannot write file (%s).", path.c_str());
}
// 0x10007390 GetModItems(id:String, mode:Number, result:Array): "additem <id> 1" lines.
void GetModItemsBody(GFxFunctionHandler::Args* a) {
    const char* text = StringArg(a, 0);
    if (!text || !TypeIs(a, 1, GFxValue::kType_Number) || !TypeIs(a, 2, GFxValue::kType_Array)) {
        Log("    ERROR: system: invalid arguments. MfgConsole.StoreItems(id:string, mode:Number, result:Array).");
        return;
    }
    const int mode = static_cast<int>(a->args[1].GetNumber());
    const unsigned long id = strtoul(text, nullptr, 16);
    TESForm* form = id && id != 0xFFFFFFFFul ? LookupFormByID(id) : nullptr;
    auto ref = Cast<TESObjectREFR>(form, RTTI_TESObjectREFR);
    auto base = ref ? static_cast<TESForm*>(const_cast<void*>(Ptr(ref, 0x24))) : nullptr;
    if (!IsForm(base)) { Console_Print("%s", text); return; }
    if (FormTypeOf(base) != 0x1C) { Console_Print("This is not container. stop."); return; }
    std::vector<std::string> names;
    EnterCriticalSection(&tableLock); names = modNames; LeaveCriticalSection(&tableLock);
    unsigned armors = 0, weapons = 0;
    char line[32];
    for (size_t i = 0; i < names.size() && i < 0xFF; ++i) {
        if (!ModeIncludes(mode, names[i].c_str())) continue;
        const std::string path = "data\\" + names[i];
        Console_Print("%s", names[i].c_str());
        FileSource file(path.c_str());
        if (!file.Open()) { Log("    ERROR: system: %s not exist", path.c_str()); continue; }
        std::vector<std::uint32_t> armor, weapon;
        ScanItems(file, std::uint8_t(i), armor, weapon);
        for (auto list : {&armor, &weapon})
            for (auto item : *list) {
                sprintf_s(line, "additem %x 1", item);
                GFxValue s; a->movie->CreateString(&s, line);
                a->args[2].PushBack(&s);
            }
        armors += unsigned(armor.size()); weapons += unsigned(weapon.size());
    }
    Console_Print("add %d armors, %d weapons.", armors, weapons);
}

template <void (*Body)(GFxFunctionHandler::Args*)>
class Function : public GFxFunctionHandler {
public:
    void Invoke(Args* a) override {
        if (disabled || !a) return;
        __try { Body(a); }
        __except (LegacyFault(GetExceptionInformation())) {}
    }
};
class GetObjectData : public Function<GetObjectDataBody> {};
class GetObjectExtraData : public Function<GetObjectExtraDataBody> {};
class EnableExtraInfo : public Function<EnableExtraInfoBody> {};
class PlayerBlinkSet : public Function<PlayerBlinkSetBody> {};
class ConsolePrint : public Function<ConsolePrintBody> {};
class SaveBatFile : public Function<SaveBatFileBody> {};
class GetModItems : public Function<GetModItemsBody> {};

bool RegisterBody(GFxMovieView* view, GFxValue* root) {
    RegisterFunction<GetObjectData>(root, view, "GetObjectData");
    RegisterFunction<SaveBatFile>(root, view, "SaveBatFile");
    RegisterFunction<ConsolePrint>(root, view, "ConsolePrint");
    RegisterFunction<EnableExtraInfo>(root, view, "EnableExtraInfo");
    RegisterFunction<PlayerBlinkSet>(root, view, "PlayerBlinkSet");
    RegisterFunction<GetModItems>(root, view, "GetModItems");
    RegisterFunction<GetObjectExtraData>(root, view, "GetObjectExtraData");
    return true;
}
bool Register(GFxMovieView* view, GFxValue* root) {
    if (!view || !root) return false;
    __try { return RegisterBody(view, root); }
    __except (LegacyFault(GetExceptionInformation())) { return false; }
}
}  // namespace

// ---------------------------------------------------------------------------------------
// ModifyFaceGen call sites (0x10003E60 / 0x10003E30 / 0x10003E10).
void __fastcall SetPhoneme(BSFaceGenAnimationData* face, void*, UInt32 id, UInt32 bits) {
    if (id >= 16 || !face) return;
    WriteFaceValue(face, 3, id, bits ? bits : 0x33D6BF95u);  // 0 -> 1e-7 (0x33D6BF95), as the original
}
void __fastcall SetModifier(BSFaceGenAnimationData* face, void*, UInt32 id, UInt32 bits) {
    if (id >= 17 || !face) return;
    WriteFaceValue(face, 2, id, bits ? bits : 0x33D6BF95u);
}
UInt32 __fastcall ResetFace(BSFaceGenAnimationData* face, void*, UInt32 a, UInt32 b, UInt32 c, UInt32 d, UInt32 e) {
    if (face) ClearFaceForReset(face);
    using Fn = UInt32 (__thiscall*)(BSFaceGenAnimationData*, UInt32, UInt32, UInt32, UInt32, UInt32);
    return reinterpret_cast<Fn>(kResetTarget)(face, a, b, c, d, e);
}
bool PatchCallSite(UInt32 site, UInt32 expected, const void* replacement) {
    auto code = reinterpret_cast<unsigned char*>(site);
    if (!Mem(code, 5) || code[0] != 0xE8) return false;
    std::int32_t rel; memcpy(&rel, code + 1, 4);
    if (site + 5 + rel != expected) return false;   // someone else's call (MfgConsole) - leave it
    DWORD protection;
    if (!VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &protection)) return false;
    const std::int32_t fresh = static_cast<std::int32_t>(reinterpret_cast<UInt32>(replacement) - (site + 5));
    memcpy(code + 1, &fresh, 4);
    DWORD ignored; VirtualProtect(code, 5, protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), code, 5);
    return true;
}

bool SetBlink(bool on) {
    if (on) return InterlockedCompareExchange(&blinkEnabled, 1, 0) == 0;
    if (InterlockedCompareExchange(&blinkEnabled, 0, 1) != 1) return false;
    InterlockedExchange(&blinkResetPending, 1);   // zeroed on the face thread (0x10003A90)
    return true;
}
void AfterFaceUpdate(BSFaceGenAnimationData* data) {
    if (disabled || (!blinkEnabled && !blinkResetPending)) return;
    __try {
        const DWORD tick = Now();
        if (tick - playerFaceAt >= 30 || !playerFaceAt) {   // refresh the player's face pointer
            Actor* player = *g_thePlayer;
            playerFace = player ? FaceOf(player) : nullptr;
            playerFaceAt = tick;
        }
        if (!playerFace || playerFace != data) return;
        if (InterlockedExchange(&blinkResetPending, 0)) { WriteBlink(data, 0.0f); blinkClock = 0; }
        if (!blinkEnabled) return;
        const DWORD now = Now();
        if (!blinkClock) blinkClock = now;
        DWORD steps = (now - blinkClock) / 30;   // the original timer period
        if (steps > 400) steps = 400;
        blinkClock += steps * 30;
        bool touched = false; float value = 0;
        for (DWORD i = 0; i < steps; ++i) {
            blinkCounter = BlinkStep(blinkCounter, rand());
            touched |= BlinkValue(blinkCounter, value);
        }
        if (touched || BlinkValue(blinkCounter, value)) WriteBlink(data, value);
    } __except (LegacyFault(GetExceptionInformation())) {}
}

namespace {
void OnDataLoadedBody() {
    auto handler = reinterpret_cast<const char*>(Ptr(reinterpret_cast<void*>(0x012E2CB8), 0));
    std::vector<std::string> fresh;
    UInt32 count = 0;
    if (handler && Get(handler, 0x69C, count) && count <= 0xFF)
        for (UInt32 i = 0; i < count; ++i) {
            auto file = reinterpret_cast<const char*>(Ptr(handler, 0x6A0 + i * 4));
            const char* s = file ? Text(file + 0x20) : nullptr;
            fresh.push_back(s ? s : "");
        }
    EnterCriticalSection(&tableLock);
    modNames.swap(fresh);
    LeaveCriticalSection(&tableLock);
    Log("Mfg Console layer: %u loaded plugins", count);
    if (extraInfo) StartScan();
}
}
void OnDataLoaded() {
    if (!tableLockReady) return;
    __try { OnDataLoadedBody(); }
    __except (LegacyFault(GetExceptionInformation())) {}
}
#ifdef MFGLE_TEST
void TestOverrides(const std::vector<Override>& table) {
    for (int i = 0; i < 5000 && scanState == 1; ++i) Sleep(1);   // let a running scan finish first
    EnterCriticalSection(&tableLock); overrides = table; std::sort(overrides.begin(), overrides.end());
    LeaveCriticalSection(&tableLock); InterlockedExchange(&scanState, 2);
}
LONG TestFaults() { return faults; }
#endif
bool Install(SKSEScaleformInterface* scaleform, bool blink, bool extra) {
    if (!InterlockedExchange(&tableLockReady, 1)) InitializeCriticalSection(&tableLock);
    InterlockedExchange(&extraInfo, extra ? 1 : 0);
    if (blink) SetBlink(true);
    if (scaleform && scaleform->Register("MfgConsole", Register)) InterlockedExchange(&replacing, 1);
    const bool phoneme = PatchCallSite(kPhonemeSite, kPhonemeTarget, reinterpret_cast<const void*>(&SetPhoneme));
    const bool modifier = PatchCallSite(kModifierSite, kModifierTarget, reinterpret_cast<const void*>(&SetModifier));
    const bool reset = PatchCallSite(kResetSite, kResetTarget, reinterpret_cast<const void*>(&ResetFace));
    InterlockedExchange(&callSites, (phoneme ? 1 : 0) | (modifier ? 2 : 0) | (reset ? 4 : 0));
    Log("Mfg Console layer: scaleform %s, call sites phoneme=%d modifier=%d reset=%d, blink=%d extra=%d",
        replacing ? "registered" : "NOT registered", phoneme, modifier, reset, blink ? 1 : 0, extra ? 1 : 0);
    return replacing && phoneme && modifier && reset;
}
}}
