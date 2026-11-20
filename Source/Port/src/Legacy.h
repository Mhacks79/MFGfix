#pragma once
// 1.0.13: Mfg Console replacement. When MfgConsole.dll is not loaded, this plugin
// provides everything it did, from the same addresses and with the same behaviour:
//   - the skse.plugins.MfgConsole Scaleform API that console.swf calls (GetObjectData,
//     GetObjectExtraData, EnableExtraInfo, PlayerBlinkSet, GetModItems, SaveBatFile,
//     ConsolePrint), so its info table, expression table and extra info window work;
//   - its three call-site fixes inside ModifyFaceGen (mfg phoneme / modifier / reset
//     write the face arrays at once, as the old plugin did);
//   - the player blink.
// When MfgConsole.dll is loaded, none of this is installed and that DLL keeps its job.
#include "skse/PluginAPI.h"
#include "skse/NiExtraData.h"

class Actor;
namespace MfgLE { namespace Legacy {
extern volatile LONG replacing;      // Scaleform API registered by this plugin
extern volatile LONG callSites;      // ModifyFaceGen call sites patched by this plugin
extern volatile LONG blinkEnabled;   // player blink running
extern volatile LONG extraInfo;      // GetObjectExtraData answers, overwrite table wanted
extern volatile LONG disabled;       // after kMaxFaults faults the console UI layer stops

// SKSEPlugin_Load: register Scaleform and patch the call sites (MfgConsole.dll absent).
bool Install(SKSEScaleformInterface* scaleform, bool blink, bool extra);
// kMessage_DataLoaded: snapshot the load order and, with extra info on, scan plugins.
void OnDataLoaded();
// Face update thread, after the engine's own update: player blink.
void AfterFaceUpdate(BSFaceGenAnimationData* data);
bool SetBlink(bool on);

// The three ModifyFaceGen call sites (TESV 1.9.32.0.8) and what they call in vanilla.
constexpr UInt32 kPhonemeSite = 0x005440BF, kPhonemeTarget = 0x005352D0;
constexpr UInt32 kModifierSite = 0x005441A6, kModifierTarget = 0x005352A0;
constexpr UInt32 kResetSite = 0x005442BD, kResetTarget = 0x0059E320;
bool PatchCallSite(UInt32 site, UInt32 expected, const void* replacement);
void __fastcall SetPhoneme(BSFaceGenAnimationData* face, void*, UInt32 id, UInt32 bits);
void __fastcall SetModifier(BSFaceGenAnimationData* face, void*, UInt32 id, UInt32 bits);
#ifdef MFGLE_TEST
struct Override;
extern DWORD (WINAPI* testClock)();
LONG TestFaults();
#endif
UInt32 __fastcall ResetFace(BSFaceGenAnimationData* face, void*, UInt32 a, UInt32 b, UInt32 c, UInt32 d, UInt32 e);
}}
