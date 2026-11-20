#include "Engine.h"
#include "Legacy.h"
#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/PapyrusNativeFunctions.h"
#include "skse/GameThreads.h"
#include "skse/CommandTable.h"
#include <shlobj.h>

const _ExtractArgs ExtractArgs=(_ExtractArgs)0x00514830;

namespace {
SKSEObjectInterface* tasks=nullptr;
bool initialized=false;
Cmd_Execute originalConsole=nullptr;
// 1.0.12: coexistence with the original Mfg Console. Its console.swf (the phoneme /
// modifier table and the extra info window) gets every value from MfgConsole.dll's
// skse.plugins.MfgConsole Scaleform functions, and its mp/me/mm aliases expect the
// plain "mfg phoneme|modifier|expression|reset" commands to apply at once. When that DLL
// is loaded, those four plain verbs are handed back to it; the ng* test verbs, status and
// every Papyrus native stay with this plugin. [Console] bPassToMfgConsole=0 turns it off.
bool passToMfgConsole=true;
// 1.0.13: without MfgConsole.dll this plugin takes its place ([MfgConsole] bReplace=1).
bool replaceMfgConsole=true, legacyBlink=true, legacyExtraInfo=true;
int legacyConsoleOverride=-1;   // the offline harness only: -1 = ask Windows
bool LegacyConsoleLoaded() {
    if(legacyConsoleOverride>=0) return legacyConsoleOverride!=0;
    return GetModuleHandleA("MfgConsole.dll")!=nullptr;
}
class CommandTask : public TaskDelegate {
    MfgLE::Request request;
public:
    explicit CommandTask(const MfgLE::Request& value):request(value) {}
    void Run() override {
        __try { MfgLE::ApplyRequest(request); }
        __except(MfgLE::Fault(GetExceptionInformation())) { }
    }
    void Dispose() override { delete this; }
};
bool Submit(Actor* actor, MfgLE::Request& request) {
    if(!initialized || MfgLE::disabled || !tasks || !MfgLE::Readable(actor,sizeof(TESForm))) return false;
    request.formID=actor->formID;
    request.generation=MfgLE::generation;
    tasks->AddTask(new CommandTask(request));
    return true;
}
bool SmoothBody(Actor* actor, SInt32 mode, SInt32 id, SInt32 value, float speed) {
    if(!MfgLE::ValidRequest(mode,id) || !std::isfinite(speed)) return false;
    MfgLE::Request request={}; request.mode=mode;request.id=id;request.value=value;
    request.speed=MfgLE::Clamp(speed,0,10);
    return Submit(actor,request);
}
bool Smooth(StaticFunctionTag*,Actor* actor,SInt32 mode,SInt32 id,SInt32 value,float speed) {
    __try { return SmoothBody(actor,mode,id,value,speed); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return false; }
}
bool ConsoleBody(COMMAND_ARGS) {
    char text[512]={}; SInt32 id=-1,value=0;
    const UInt32 saved=opcodeOffsetPtr?*opcodeOffsetPtr:0;
    if(!ExtractArgs(paramInfo,scriptData,opcodeOffsetPtr,thisObj,containingObj,scriptObj,locals,text,&id,&value)) return false;
    const bool plainVerb=!_stricmp(text,"phoneme") || !_stricmp(text,"modifier") ||
                         !_stricmp(text,"expression") || !_stricmp(text,"reset");
    // The plain verbs go to the engine's own ModifyFaceGen, whose three broken calls are
    // fixed either by MfgConsole.dll or, without it, by this plugin (Legacy::callSites==7).
    if(plainVerb && passToMfgConsole && (LegacyConsoleLoaded() || MfgLE::Legacy::callSites==7)) {
        MfgLE::Log("console %s: live edit through ModifyFaceGen (%s)",text,
            LegacyConsoleLoaded()?"MfgConsole.dll":"Mfg Fix NG LE call-site fixes");
        if(opcodeOffsetPtr) *opcodeOffsetPtr=saved;
        return originalConsole(paramInfo,scriptData,thisObj,containingObj,scriptObj,locals,result,opcodeOffsetPtr);
    }
    auto ref=thisObj?thisObj:*g_thePlayer;
    auto actor=ref && ref->formType==kFormType_Character?static_cast<Actor*>(ref):nullptr;
    int mode=99;float speed=MfgLE::defaultSpeed;
    if(!_stricmp(text,"phoneme")) mode=0;
    else if(!_stricmp(text,"modifier")) mode=1;
    else if(!_stricmp(text,"expression")) mode=2;
    else if(!_stricmp(text,"reset")) mode=-1;
    else if(!_stricmp(text,"ngsmile")) { mode=2;id=10;value=100;speed=1.5f; }
    else if(!_stricmp(text,"nganger")) { mode=2;id=8;value=100;speed=1.5f; }
    else if(!_stricmp(text,"ngmoodzero")) { mode=2;id=0;value=100;speed=1.5f; }
    else if(!_stricmp(text,"ngmouth")) { mode=0;id=0;value=100;speed=1.5f; }
    else if(!_stricmp(text,"ngbrows")) { mode=1;id=6;value=100;speed=1.5f; }
    else if(!_stricmp(text,"ngreset")) { mode=-1;speed=1.5f; }
    else if(!_stricmp(text,"ngstatus") || !_stricmp(text,"status") || !_stricmp(text,"statut")) {
        Console_Print("Mfg Fix NG LE: %s; expression ID=%d; dialogue=%s",
            initialized && !MfgLE::disabled?"ready":"disabled",MfgLE::GetValue(actor,3,0),MfgLE::Dialogue(actor)?"yes":"no");
        if(result) *result=1;return true;
    }
    if(mode!=99 && (mode==-1 || id!=-1)) {
        const bool success=SmoothBody(actor,mode,id,value,speed);
        MfgLE::Log("console %s: actor=%08X %s",text,actor?actor->formID:0,success?"queued":"rejected");
        Console_Print("Mfg Fix NG LE: %s",success?"request queued (close console to animate)":"request rejected");
        if(result) *result=success?1:0;return true;
    }
    // Preserve vanilla info/custom commands and unknown commands. ExtractArgs
    // advances the script offset, so restore it before handing to the old handler.
    if(opcodeOffsetPtr) *opcodeOffsetPtr=saved;
    return originalConsole(paramInfo,scriptData,thisObj,containingObj,scriptObj,locals,result,opcodeOffsetPtr);
}
bool ConsoleHook(COMMAND_ARGS) {
    __try { return ConsoleBody(paramInfo,scriptData,thisObj,containingObj,scriptObj,locals,result,opcodeOffsetPtr); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return false; }
}
bool InstallConsole() {
    auto command=reinterpret_cast<CommandInfo*>(0x0124D4D0);
    if(!MfgLE::Readable(command,sizeof(*command)) || !command->execute ||
       !command->longName || strcmp(command->longName,"ModifyFaceGen")) return false;
    DWORD protection;
    if(!VirtualProtect(&command->execute,sizeof(command->execute),PAGE_READWRITE,&protection)) return false;
    originalConsole=command->execute;command->execute=ConsoleHook;
    DWORD ignored;VirtualProtect(&command->execute,sizeof(command->execute),protection,&ignored);
    return true;
}
bool Set(StaticFunctionTag* tag,Actor* actor,SInt32 mode,SInt32 id,SInt32 value) {
    return Smooth(tag,actor,mode,id,value,MfgLE::defaultSpeed);
}
SInt32 Get(StaticFunctionTag*,Actor* actor,SInt32 mode,SInt32 id) {
    __try { return MfgLE::GetValue(actor,mode,id); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return -1; }
}
bool Reset(StaticFunctionTag*,Actor* actor,SInt32 mode,float speed) {
    __try {
        if((mode!=-1 && mode!=0 && mode!=1) || !std::isfinite(speed)) return false;
        MfgLE::Request request={}; request.mode=mode==-1?-1:mode==0?-2:-3;
        request.speed=MfgLE::Clamp(speed,0,10);
        return Submit(actor,request);
    } __except(MfgLE::Fault(GetExceptionInformation())) { return false; }
}
bool PresetBody(Actor* actor,VMArray<float> values,bool open,SInt32 power,float expression,float modifier,float phoneme,float speed) {
    if(values.Length()!=32 || !std::isfinite(speed) || !std::isfinite(expression) ||
       !std::isfinite(modifier) || !std::isfinite(phoneme)) return false;
    MfgLE::Request request={}; request.preset=true;request.openMouth=open;request.speed=MfgLE::Clamp(speed,0,10);
    for(unsigned j=0;j<32;++j) { values.Get(request.values+j,j); if(!std::isfinite(request.values[j])) return false; }
    float mood=request.values[30];
    if(mood<0 || mood>16 || mood!=std::floor(mood)) return false;
    for(unsigned j=0;j<30;++j) request.values[j]=MfgLE::Clamp(request.values[j]*MfgLE::Clamp(j<16?phoneme:modifier,0,200),0,2);
    request.values[31]=MfgLE::Clamp(request.values[31]*MfgLE::Clamp(expression,0,200),0,2);
    if(mood>0 && request.values[31]==0) request.values[31]=MfgLE::Clamp(power/100.0f,0,2);
    return Submit(actor,request);
}
bool Preset(StaticFunctionTag*,Actor* actor,VMArray<float> values,bool open,SInt32 power,float expression,float modifier,float phoneme,float speed) {
    __try { return PresetBody(actor,values,open,power,expression,modifier,phoneme,speed); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return false; }
}
bool InDialogue(StaticFunctionTag*,Actor* actor) {
    __try { return MfgLE::Dialogue(actor); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return false; }
}
Actor* Target(StaticFunctionTag*) {
    __try { return MfgLE::SpeechTarget(); }
    __except(MfgLE::Fault(GetExceptionInformation())) { return nullptr; }
}
bool Register(VMClassRegistry* vm) {
    vm->RegisterFunction(new NativeFunction4<StaticFunctionTag,bool,Actor*,SInt32,SInt32,SInt32>("SetPhonemeModifier","MfgConsoleFunc",Set,vm));
    vm->RegisterFunction(new NativeFunction3<StaticFunctionTag,SInt32,Actor*,SInt32,SInt32>("GetPhonemeModifier","MfgConsoleFunc",Get,vm));
    vm->RegisterFunction(new NativeFunction5<StaticFunctionTag,bool,Actor*,SInt32,SInt32,SInt32,float>("SetPhonemeModifierSmooth","MfgConsoleFuncExt",Smooth,vm));
    vm->RegisterFunction(new NativeFunction3<StaticFunctionTag,bool,Actor*,SInt32,float>("ResetMFGSmooth","MfgConsoleFuncExt",Reset,vm));
    vm->RegisterFunction(new NativeFunction8<StaticFunctionTag,bool,Actor*,VMArray<float>,bool,SInt32,float,float,float,float>("ApplyExpressionPreset","MfgConsoleFuncExt",Preset,vm));
    vm->RegisterFunction(new NativeFunction0<StaticFunctionTag,Actor*>("GetPlayerSpeechTarget","MfgConsoleFuncExt",Target,vm));
    vm->RegisterFunction(new NativeFunction1<StaticFunctionTag,bool,Actor*>("IsInDialogue","MfgConsoleFuncExt",InDialogue,vm));
    MfgLE::Log("registered 7 MfgConsoleFunc / MfgConsoleFuncExt native functions");
    return true;
}
void Message(SKSEMessagingInterface::Message* msg) {
    __try {
        if(!initialized || !msg) return;
        if(msg->type==SKSEMessagingInterface::kMessage_DataLoaded && MfgLE::Legacy::replacing) MfgLE::Legacy::OnDataLoaded();
        if(msg->type==SKSEMessagingInterface::kMessage_PreLoadGame ||
           msg->type==SKSEMessagingInterface::kMessage_PostLoadGame ||
           msg->type==SKSEMessagingInterface::kMessage_NewGame) MfgLE::ClearState();
    } __except(MfgLE::Fault(GetExceptionInformation())) { }
}
bool LogSwitchedOn() {
    char path[MAX_PATH]; const DWORD length=GetModuleFileNameA(nullptr,path,MAX_PATH);
    if(!length || length>=MAX_PATH) return false;
    char* end=strrchr(path,'\\'); if(!end) return false; *end=0;
    if(strcat_s(path,"\\Data\\SKSE\\Plugins\\MfgFixNGLE.ini")) return false;
    char value[16]=""; GetPrivateProfileStringA("Debug","EnableLog","0",value,sizeof(value),path);
    return value[0]=='1' || !_stricmp(value,"true");
}
void Config() {
    char path[MAX_PATH]; GetModuleFileNameA(nullptr,path,MAX_PATH);
    char* end=strrchr(path,'\\'); if(!end) return; *end=0;
    strcat_s(path,"\\Data\\SKSE\\Plugins\\MfgFixNGLE.ini");
    char text[64]; GetPrivateProfileStringA("Transition","fDefaultSpeed","0",text,sizeof(text),path);
    float value=0;
    if(MfgLE::Decimal(text,value)) MfgLE::defaultSpeed=MfgLE::Clamp(value,0,10);
    char pass[16]; GetPrivateProfileStringA("Console","bPassToMfgConsole","1",pass,sizeof(pass),path);
    passToMfgConsole=!(pass[0]=='0' || !_stricmp(pass,"false"));
    auto flag=[&](const char* key) {
        char value[16]; GetPrivateProfileStringA("MfgConsole",key,"1",value,sizeof(value),path);
        return !(value[0]=='0' || !_stricmp(value,"false"));
    };
    replaceMfgConsole=flag("bReplace"); legacyBlink=flag("bPlayerBlink"); legacyExtraInfo=flag("bExtraInfo");
}
}
extern "C" bool SKSEPlugin_Query(const SKSEInterface* skse,PluginInfo* info) {
    info->infoVersion=PluginInfo::kInfoVersion;info->name="Mfg Fix NG LE";info->version=0x01000D;
    return !skse->isEditor && skse->runtimeVersion==RUNTIME_VERSION_1_9_32_0;
}
extern "C" bool SKSEPlugin_Load(const SKSEInterface* skse) {
    __try {
        MfgLE::loadThread=GetCurrentThreadId();
        if(LogSwitchedOn()) InterlockedExchange(&MfgLE::logEnabled,1);
        MfgLE::Log("Mfg Fix NG LE 1.0.13; runtime %08X; MfgConsole.dll %s",skse->runtimeVersion,
            LegacyConsoleLoaded()?"loaded":"not loaded");
        tasks=static_cast<SKSEObjectInterface*>(skse->QueryInterface(kInterface_Task));
        auto papyrus=static_cast<SKSEPapyrusInterface*>(skse->QueryInterface(kInterface_Papyrus));
        auto messages=static_cast<SKSEMessagingInterface*>(skse->QueryInterface(kInterface_Messaging));
        if(!tasks || !papyrus || !messages) return false;
        Config();
        if(!MfgLE::InstallHooks()) return false;
        initialized=true;
        if(!LegacyConsoleLoaded() && replaceMfgConsole) {
            auto scaleform=static_cast<SKSEScaleformInterface*>(skse->QueryInterface(kInterface_Scaleform));
            if(!MfgLE::Legacy::Install(scaleform,legacyBlink,legacyExtraInfo))
                MfgLE::Log("Mfg Console replacement incomplete (see the line above); face fixes unaffected");
        }
        if(!InstallConsole() || !papyrus->Register(Register) || !messages->RegisterListener(skse->GetPluginHandle(),"SKSE",Message)) {
            MfgLE::disabled=1;
            // Hooks stay valid even if registration fails. Do not unload their code.
            MfgLE::Log("registration failed; disabled and retained to keep hooks valid");
        }
        return true;
    } __except(MfgLE::Fault(GetExceptionInformation())) { return initialized; }
}
