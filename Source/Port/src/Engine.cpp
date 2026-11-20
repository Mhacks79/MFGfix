#include "Engine.h"
#include "Signatures.h"
#include "Legacy.h"
#include "skse/GameAPI.h"
#include "skse/GameRTTI.h"
#include "skse/PapyrusVM.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <shlobj.h>


namespace MfgLE {
float defaultSpeed = 0.0f;
volatile LONG disabled = 0;
volatile LONG generation = 0;
DWORD loadThread = 0;
volatile LONG logEnabled = 0;
namespace {
struct Transition {
    BSFaceGenAnimationData* data;
    std::uint64_t mask;
    float target[47];
    float duration;
    bool releaseExpression;
};
Transition states[Slots] = {};
volatile LONG activeStates = 0;
CRITICAL_SECTION stateLock;
volatile LONG faults = 0;
volatile LONG threadLogged = 0;
using UpdateFn = bool (__thiscall*)(BSFaceGenAnimationData*, float, bool);
using DestroyFn = void (__thiscall*)(BSFaceGenAnimationData*);
UpdateFn originalUpdate = nullptr;
DestroyFn originalDestroy = nullptr;

SimpleLock* FaceLock(BSFaceGenAnimationData* data) {
    return reinterpret_cast<SimpleLock*>(reinterpret_cast<char*>(data) + 0x1A8);
}
void* Link(const void* object, unsigned offset) {
    if (!Readable(object,offset+sizeof(void*))) return nullptr;
    auto pointer=*reinterpret_cast<void* const*>(reinterpret_cast<const char*>(object)+offset);
    return Readable(pointer,sizeof(void*))?pointer:nullptr;
}
BSFaceGenAnimationData* FaceData(Actor* actor) {
    // 0x731590 -> 0x71FCE0 -> 0x730AD0 proves this path. Avoid the
    // getter's fallback, which can rebuild a face from the Papyrus thread.
    auto process=Link(actor,0x88);
    auto middle=Link(process,0x04);
    auto node=Link(middle,0xE0);
    return static_cast<BSFaceGenAnimationData*>(Link(node,0xDC));
}
bool Ready(BSFaceGenAnimationData* data) {
    if (!Readable(data, sizeof(*data))) return false;
    // Only source arrays touched by this port: expression, modifier, phoneme.
    const unsigned index[] = {0, 1, 2, 3};
    const unsigned expected[] = {17, 17, 17, 16};
    for (unsigned j=0; j<4; ++j) {
        auto& key = data->keyFrames[index[j]];
        if (key.count != expected[j] || !Readable(key.values, key.count*sizeof(float))) return false;
    }
    return true;
}
Transition* Find(BSFaceGenAnimationData* data, bool create) {
    Transition* empty = nullptr;
    for (auto& state : states) {
        if (state.data == data) return &state;
        if (!state.data && !empty) empty = &state;
    }
    if (create && empty) { *empty = {}; empty->data = data; InterlockedIncrement(&activeStates); }
    return create ? empty : nullptr;
}
float* Value(BSFaceGenAnimationData* data, unsigned channel) {
    if (channel < 16) return data->keyFrames[3].values + channel;
    if (channel < 30) return data->keyFrames[2].values + channel-16;
    return data->keyFrames[1].values + channel-30;
}
void Target(Transition& state, unsigned channel, float target) {
    state.mask |= (std::uint64_t(1) << channel);
    state.target[channel] = Clamp(target, 0, 2);
}
void Expression(Transition& state, unsigned id, float target) {
    // The engine may still own an expression-transition allocation at +0x0C.
    // Release it through its own pool helper before taking over source values.
    auto transition=reinterpret_cast<void**>(reinterpret_cast<char*>(state.data)+0x0C);
    if (*transition) {
        using ReleaseFn=void (__cdecl*)(void**,void*);
        reinterpret_cast<ReleaseFn>(0x0059D870)(transition,reinterpret_cast<void*>(0x01B1A470));
    }
    for (unsigned j=0; j<17; ++j) Target(state, 30+j, j==id ? target : 0);
    state.data->overrideFlag = 1;
    state.releaseExpression = false;
}
void Advance(BSFaceGenAnimationData* data, Transition& state, float dt) {
    bool activeExpression = false;
    for (unsigned channel=0; channel<47; ++channel) {
        if (!(state.mask & (std::uint64_t(1) << channel))) continue;
        auto value = Value(data, channel);
        *value = Step(Clamp(*value, 0, 2), state.target[channel], dt, state.duration);
        auto& key = data->keyFrames[channel<16 ? 3 : channel<30 ? 2 : 1];
        key.isUpdated = 0;
        if (channel>=30 && *value != 0) activeExpression = true;
        if (*value == state.target[channel]) state.mask &= ~(std::uint64_t(1) << channel);
    }
    if (state.releaseExpression && !activeExpression && !(state.mask >> 30)) {
        data->overrideFlag = 0;
        state.releaseExpression = false;
    }
    // Keep even an immediate/zero-valued request until a positive native tick.
    // Native Update consumes the force edge at dt=0 without rebuilding arrays.
}
void Tick(BSFaceGenAnimationData* data, float dt) {
    if (!Readable(data, sizeof(*data))) return;
    auto lock = FaceLock(data);
    lock->Lock();
    __try {
        EnterCriticalSection(&stateLock);
        __try {
            if (Ready(data) && !data->unk1A2 && dt>0) {
                auto state = Find(data, false);
                if (state) {
                    Advance(data, *state, dt);
                    // 0x0059E5C0 copies explicit sources into final frames on
                    // this edge, including zeros. isUpdated alone is insufficient.
                    data->unk1A3=1;data->unk1A4=0;
                    if (!state->mask) { *state={}; InterlockedDecrement(&activeStates); }
                }
            }
        } __finally { LeaveCriticalSection(&stateLock); }
    } __finally { lock->Release(); }
}
bool __fastcall UpdateHook(BSFaceGenAnimationData* data, void*, float dt, bool blink) {
    if (!InterlockedCompareExchange(&threadLogged, 1, 0))
        Log("face update thread %u; plugin load thread %u", GetCurrentThreadId(), loadThread);
    if (!disabled && activeStates>0) {
        __try { Tick(data, dt); }
        __except(Fault(GetExceptionInformation())) { }
    }
    // The original LE engine owns dialogue, blinking, eye tracking and frozen actors.
    // Do not swallow faults originating inside the original engine implementation.
    const bool result = originalUpdate(data, dt, blink);
    Legacy::AfterFaceUpdate(data);   // player blink of the Mfg Console replacement (own guard)
    return result;
}
void __fastcall DestroyHook(BSFaceGenAnimationData* data, void*) {
    __try {
        EnterCriticalSection(&stateLock);
        __try { auto state=Find(data, false); if (state) { *state={}; InterlockedDecrement(&activeStates); } }
        __finally { LeaveCriticalSection(&stateLock); }
    } __except(Fault(GetExceptionInformation())) { }
    originalDestroy(data);
}
// These two proven prologues contain no PC-relative instructions. Do not use
// this small trampoline builder for any other entry without disassembly proof.
void* Trampoline(UInt32 address, unsigned span) {
    auto code=static_cast<unsigned char*>(VirtualAlloc(nullptr, span+5, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
    if (!code) return nullptr;
    memcpy(code, reinterpret_cast<void*>(address), span);
    code[span]=0xE9;
    *reinterpret_cast<std::int32_t*>(code+span+1)=static_cast<std::int32_t>(address+span - reinterpret_cast<UInt32>(code+span+5));
    DWORD old;
    if (!VirtualProtect(code, span+5, PAGE_EXECUTE_READ, &old)) { VirtualFree(code,0,MEM_RELEASE); return nullptr; }
    FlushInstructionCache(GetCurrentProcess(), code, span+5);
    return code;
}
void Patch(UInt32 address, void* hook, unsigned span) {
    unsigned char bytes[16]; memset(bytes,0x90,sizeof(bytes)); bytes[0]=0xE9;
    *reinterpret_cast<std::int32_t*>(bytes+1)=static_cast<std::int32_t>(reinterpret_cast<UInt32>(hook)-address-5);
    memcpy(reinterpret_cast<void*>(address), bytes, span);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),span);
}
}

void Log(const char* format, ...) {
#ifndef MFGLE_TEST
    if (!logEnabled) return;
#endif
    char message[1024]; va_list args; va_start(args,format);
    vsnprintf_s(message,sizeof(message),_TRUNCATE,format,args); va_end(args);
#ifdef MFGLE_TEST
    puts(message);
#else
    char path[MAX_PATH];
    if (!SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, path)) {
        strcat_s(path,"\\My Games\\Skyrim\\SKSE\\MfgFixNGLE.log");
        FILE* file=nullptr;
        if (!fopen_s(&file,path,"ab") && file) { fprintf(file,"%s\r\n",message); fclose(file); }
    }
#endif
}
int Fault(EXCEPTION_POINTERS* info) {
    LONG count=InterlockedIncrement(&faults);
    Log("FAULT %ld: code %08X at %p; plugin disabled",count,
        info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress);
    // Disable on the first fault. This plugin never takes player controls.
    InterlockedExchange(&disabled,1);
    return EXCEPTION_EXECUTE_HANDLER;
}
bool Readable(const void* pointer, size_t size) {
    auto start=reinterpret_cast<std::uintptr_t>(pointer);
    if (!start || (start&3) || start>=0xFFFF0000 || size>0xFFFF0000-start) return false;
    auto end=start+size;
    while(start<end) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery(reinterpret_cast<void*>(start),&info,sizeof(info)) ||
            info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
        if (!(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return false;
        auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if(next<=start) return false;
        start=next;
    }
    return true;
}
void ClearState() {
    EnterCriticalSection(&stateLock);
    __try { InterlockedIncrement(&generation); memset(states,0,sizeof(states)); InterlockedExchange(&activeStates,0); }
    __finally { LeaveCriticalSection(&stateLock); }
}
void ApplyRequest(const Request& request) {
    if(disabled || request.generation!=generation) return;
    auto form=LookupFormByID(request.formID);
    if(!Readable(form,sizeof(TESForm)) || form->formType!=kFormType_Character) { Log("request %08X rejected: actor unavailable",request.formID); return; }
    auto actor=static_cast<Actor*>(form);
    auto data=FaceData(actor);
    if(!Readable(data,sizeof(*data))) { Log("request %08X rejected: face chain unavailable",request.formID); return; }
    auto lock=FaceLock(data); lock->Lock();
    __try {
        if(!Ready(data)) { Log("request %08X rejected: face source arrays incomplete",request.formID); return; }
#ifndef MFGLE_TEST
        Log("apply actor=%08X mode=%d id=%d value=%d speed-ms=%d frozen=%u expression-source=1",
            request.formID,request.mode,request.id,request.value,static_cast<int>(request.speed*1000),data->unk1A2);
#endif
        EnterCriticalSection(&stateLock);
        __try {
            if(request.generation!=generation) return;
            auto state=Find(data,true);
            if(!state) { Log("transition capacity reached (%u); request rejected",Slots); return; }
            state->duration=Clamp(request.speed,0,10);
            if(request.preset) {
                if(!request.openMouth) for(unsigned j=0;j<16;++j) Target(*state,j,request.values[j]);
                for(unsigned j=0;j<14;++j) Target(*state,16+j,request.values[16+j]);
                Expression(*state,static_cast<unsigned>(request.values[30]),request.values[31]);
            } else if(request.mode==-1) {
                for(unsigned j=0;j<47;++j) Target(*state,j,0);
                state->releaseExpression=true;
            } else if(request.mode==-2 || request.mode==-3) {
                unsigned first=request.mode==-2?0:16, last=request.mode==-2?16:30;
                for(unsigned j=first;j<last;++j) Target(*state,j,0);
            } else if(request.mode==2) Expression(*state,request.id,Clamp(request.value/100.0f,0,2));
            else Target(*state,(request.mode==1?16:0)+request.id,Clamp(request.value/100.0f,0,2));
            if(state->duration==0) Advance(data,*state,0);
        } __finally { LeaveCriticalSection(&stateLock); }
    } __finally { lock->Release(); }
}
int GetValue(Actor* actor, int mode, int id) {
    if(disabled || !Readable(actor,sizeof(TESForm))) return -1;
    auto data=FaceData(actor);
    if(!Readable(data,sizeof(*data))) return -1;
    auto lock=FaceLock(data); lock->Lock();
    int result=-1;
    __try {
        if(!Ready(data)) return -1;
        if(mode==3) {
            unsigned best=7; auto& key=data->keyFrames[data->overrideFlag?1:0];
            for(unsigned j=0;j<17;++j) if(key.values[j]>key.values[best]) best=j;
            result=best;
        } else if(ValidRequest(mode,id) && mode>=0) {
            float val=mode==2 && !data->overrideFlag && Readable(data->keyFrames[0].values,17*sizeof(float)) && data->keyFrames[0].count==17
                ? data->keyFrames[0].values[id] : *Value(data,(mode==0?0:mode==1?16:30)+id);
            result=static_cast<int>(std::lround(Clamp(val,0,2)*100));
        }
    } __finally { lock->Release(); }
    return result;
}
bool Dialogue(Actor* actor) {
    if(!Readable(actor,sizeof(TESForm))) return false;
    auto data=FaceData(actor);
    if(!Readable(data,sizeof(*data))) return false;
    auto lock=FaceLock(data); lock->Lock();
    bool result=false;
    __try { result=*reinterpret_cast<void**>(reinterpret_cast<char*>(data)+0x1B0)!=nullptr; }
    __finally { lock->Release(); }
    return result;
}
Actor* SpeechTarget() {
    auto manager=*reinterpret_cast<char**>(0x013105D8);
    if(!Readable(manager,0x3C)) return nullptr;
    UInt32 handle=*reinterpret_cast<UInt32*>(manager+0x38);
    TESObjectREFR* ref=nullptr;
    if(!handle || handle==*g_invalidRefHandle || !LookupREFRByHandle(&handle,&ref) || !ref) return nullptr;
    Actor* actor=ref->formType==kFormType_Character?static_cast<Actor*>(ref):nullptr;
    ref->handleRefObject.DecRefHandle();
    return actor;
}
BSFaceGenAnimationData* FaceOf(Actor* actor) {
    return Readable(actor,sizeof(TESForm)) ? FaceData(actor) : nullptr;
}
namespace {
bool KeyOK(BSFaceGenAnimationData* data, unsigned index, unsigned count) {
    auto& key=data->keyFrames[index];
    return key.count==count && Readable(key.values,count*sizeof(float));
}
void Forget(BSFaceGenAnimationData* data, int channel) {
    // A console write wins over a smooth transition still running on the same channel.
    EnterCriticalSection(&stateLock);
    __try {
        auto state=Find(data,false);
        if(state) {
            if(channel<0) state->mask=0; else state->mask&=~(std::uint64_t(1)<<channel);
            if(!state->mask) { *state={}; InterlockedDecrement(&activeStates); }
        }
    } __finally { LeaveCriticalSection(&stateLock); }
}
}
// MfgConsole's GetObjectData reads phonemes [+0x64] (16), modifiers [+0x4C] (17) and the
// expression row [+0x94] (keyframe 5). Copied under the face lock.
bool CopyFace(BSFaceGenAnimationData* data, float* phoneme, float* modifier, float* expression, bool& expressionOK) {
    if(!Readable(data,sizeof(*data))) return false;
    auto lock=FaceLock(data); lock->Lock();
    bool ok=false;
    __try {
        if(KeyOK(data,3,16) && KeyOK(data,2,17)) {
            memcpy(phoneme,data->keyFrames[3].values,16*sizeof(float));
            memcpy(modifier,data->keyFrames[2].values,17*sizeof(float));
            expressionOK=data->keyFrames[5].count>=17 && Readable(data->keyFrames[5].values,17*sizeof(float));
            if(expressionOK) memcpy(expression,data->keyFrames[5].values,17*sizeof(float));
            ok=true;
        }
    } __finally { lock->Release(); }
    return ok;
}
// mfg phoneme / modifier, as MfgConsole's call-site fixes: value in place, isUpdated=0.
void WriteFaceValue(BSFaceGenAnimationData* data, unsigned keyframe, unsigned id, UInt32 bits) {
    if(!Readable(data,sizeof(*data)) || (keyframe!=2 && keyframe!=3)) return;
    const unsigned count=keyframe==3?16:17;
    if(id>=count) return;
    auto lock=FaceLock(data); lock->Lock();
    __try {
        if(KeyOK(data,keyframe,count)) {
            memcpy(data->keyFrames[keyframe].values+id,&bits,4);
            data->keyFrames[keyframe].isUpdated=0;
            const int channel=keyframe==3?int(id):id<14?int(16+id):-2;
            if(channel!=-2) Forget(data,channel);
        }
    } __finally { lock->Release(); }
}
// mfg reset (0x10003DB0): zero phonemes and modifiers, force the copy edge, then vanilla.
void ClearFaceForReset(BSFaceGenAnimationData* data) {
    if(!Readable(data,sizeof(*data))) return;
    auto lock=FaceLock(data); lock->Lock();
    __try {
        if(KeyOK(data,2,17) && KeyOK(data,3,16)) {
            memset(data->keyFrames[3].values,0,16*sizeof(float));
            memset(data->keyFrames[2].values,0,17*sizeof(float));
            data->unk1A3=1;data->unk1A4=0;
            Forget(data,-1);
        }
    } __finally { lock->Release(); }
}
// Player blink: keyframe 7 [+0xC4] entries 0 and 1 (count +0xC8 == 17), timer 0x100030D0.
void WriteBlink(BSFaceGenAnimationData* data, float value) {
    if(!Readable(data,sizeof(*data))) return;
    auto lock=FaceLock(data); lock->Lock();
    __try {
        if(KeyOK(data,7,17)) { data->keyFrames[7].values[0]=value; data->keyFrames[7].values[1]=value; }
    } __finally { lock->Release(); }
}
bool InstallHooks() {
    InitializeCriticalSection(&stateLock);
    if(!VerifySignatures()) return false;
    auto update=Trampoline(UpdateAddress,6), destroy=Trampoline(DestroyAddress,10);
    if(!update || !destroy) { if(update) VirtualFree(update,0,MEM_RELEASE); if(destroy) VirtualFree(destroy,0,MEM_RELEASE); return false; }
    DWORD updateProtection,destroyProtection;
    if(!VirtualProtect(reinterpret_cast<void*>(UpdateAddress),6,PAGE_EXECUTE_READWRITE,&updateProtection)) return false;
    if(!VirtualProtect(reinterpret_cast<void*>(DestroyAddress),10,PAGE_EXECUTE_READWRITE,&destroyProtection)) {
        DWORD ignored; VirtualProtect(reinterpret_cast<void*>(UpdateAddress),6,updateProtection,&ignored); return false;
    }
    originalUpdate=reinterpret_cast<UpdateFn>(update); originalDestroy=reinterpret_cast<DestroyFn>(destroy);
    Patch(UpdateAddress,reinterpret_cast<void*>(UpdateHook),6);
    Patch(DestroyAddress,reinterpret_cast<void*>(DestroyHook),10);
    DWORD ignored;
    VirtualProtect(reinterpret_cast<void*>(UpdateAddress),6,updateProtection,&ignored);
    VirtualProtect(reinterpret_cast<void*>(DestroyAddress),10,destroyProtection,&ignored);
    Log("LE face hooks installed; extended API ready; transition slots=%u",Slots);
    return true;
}
}
