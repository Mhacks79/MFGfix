#pragma once
#include "skse/GameReferences.h"
#include "skse/NiExtraData.h"
#include "skse/GameTypes.h"
#include "Core.h"

namespace MfgLE {
constexpr UInt32 UpdateAddress = 0x0059E5C0;
constexpr UInt32 DestroyAddress = 0x0059E020;
constexpr UInt32 ExpressionAddress = 0x0059DB90;
constexpr UInt32 ResetAddress = 0x0059E320;
constexpr unsigned Slots = 256;
struct Request {
    UInt32 formID;
    LONG generation;
    int mode, id, value;
    float speed;
    bool preset, openMouth;
    float values[32];
};
void Log(const char* format, ...);
int Fault(EXCEPTION_POINTERS* info);
bool Readable(const void* data, size_t size);
bool InstallHooks();
void ClearState();
void ApplyRequest(const Request& request);
int GetValue(Actor* actor, int mode, int id);
bool Dialogue(Actor* actor);
Actor* SpeechTarget();
// Mfg Console replacement helpers (Legacy.cpp), all under the face lock.
BSFaceGenAnimationData* FaceOf(Actor* actor);
bool CopyFace(BSFaceGenAnimationData* data, float* phoneme, float* modifier, float* expression, bool& expressionOK);
void WriteFaceValue(BSFaceGenAnimationData* data, unsigned keyframe, unsigned id, UInt32 bits);
void ClearFaceForReset(BSFaceGenAnimationData* data);
void WriteBlink(BSFaceGenAnimationData* data, float value);
extern float defaultSpeed;
extern volatile LONG disabled;
extern volatile LONG generation;
extern DWORD loadThread;
// Off by default: Log() writes nothing, and creates no file, until SKSEPlugin_Load
// has read [Debug] EnableLog=1 (or true) from MfgFixNGLE.ini.
extern volatile LONG logEnabled;
}
