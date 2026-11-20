#include "../src/Engine.h"
#include "../src/Signatures.h"
#include "../../harness/core/fakegame.h"
#include <cstdio>
#include <cstring>
#include <limits>
#define Console_Print printf
#include "../src/main.cpp"
#undef Console_Print

static const unsigned long blocks[]={0x00400000,0x00450000,0x004A0000,0x00510000,0x00540000,0x00590000,0x00900000,0x00920000,
    0x006A0000,0x00F50000,0x01240000,0x012E0000,0x01310000,0x01B10000,0x01B20000,0x01B40000};
FAKEGAME_CLAIM(blocks)
static unsigned char actorBytes[sizeof(Actor)];
static unsigned char faceBytes[sizeof(BSFaceGenAnimationData)];
static UInt32 vtable[256];
static UInt32 process[0x70/4], middle[0xE4/4], node[0xE0/4];
static float primaryExpression[17],expression[17],modifier[17],phoneme[16];
static float renderedExpression[17],renderedModifier[17],renderedPhoneme[16];
static bool lastMerge=false;
// Model the no-dialogue/no-transition branch verified in TESV 0x0059E5C0.
// Source zeros are NOT copied without the +1A3/+1A4 edge. This deliberately
// retains cached final values, reproducing the test1 failure instead of hiding it.
static bool __cdecl Merge(BSFaceGenAnimationData* data,float dt) {
    bool dirty=false;
    if(dt>0 && !data->unk1A2) {
        const bool force=data->unk1A3 && !data->unk1A4;
        const unsigned sources[]={1,2,3},destinations[]={5,7,9};
        for(unsigned j=0;j<3;++j) {
            auto& source=data->keyFrames[sources[j]];
            bool nonzero=false;
            for(unsigned i=0;i<source.count;++i) nonzero|=source.values[i]!=0;
            if(force || nonzero) {
                memcpy(data->keyFrames[destinations[j]].values,source.values,source.count*sizeof(float));
                dirty=true;
            }
        }
    }
    data->unk1A4=data->unk1A3;
    lastMerge=dirty;return dirty;
}
static Actor* actor=reinterpret_cast<Actor*>(actorBytes);
static BSFaceGenAnimationData* face=reinterpret_cast<BSFaceGenAnimationData*>(faceBytes);
static unsigned badSelf=0,lookupCalls=0;
static unsigned queued=0,originalCalls=0;
static TaskDelegate* pending=nullptr;
static SKSEPapyrusInterface::RegisterFunctions registration=nullptr;
static SKSEMessagingInterface::EventCallback listener=nullptr;
static void Queue(TaskDelegate* task) { ++queued; if(pending) { pending->Run();pending->Dispose(); } pending=task; }
static void Drain() { if(pending) { auto task=pending;pending=nullptr;task->Run();task->Dispose(); } }
static bool RegisterMock(SKSEPapyrusInterface::RegisterFunctions callback) { registration=callback;return true; }
static bool ListenMock(PluginHandle,const char*,SKSEMessagingInterface::EventCallback callback) { listener=callback;return true; }
static SKSEObjectInterface objectAPI={1,Queue,nullptr};
static SKSEPapyrusInterface papyrusAPI={1,RegisterMock};
static SKSEMessagingInterface messageAPI={};
namespace Fake { extern SKSEScaleformInterface scaleform; }
static void* InterfaceMock(UInt32 id) {
    return id==kInterface_Task?static_cast<void*>(&objectAPI):
           id==kInterface_Papyrus?static_cast<void*>(&papyrusAPI):
           id==kInterface_Messaging?static_cast<void*>(&messageAPI):
           id==kInterface_Scaleform?static_cast<void*>(&Fake::scaleform):nullptr;
}
static PluginHandle HandleMock() { return 1; }
struct ConsoleInput { const char* text;int id,value; };
static bool ExtractRealMock(ParamInfo*,void* script,UInt32* offset,TESObjectREFR*,TESObjectREFR*,Script*,ScriptLocals* locals,...) {
    auto input=static_cast<ConsoleInput*>(script);va_list args;va_start(args,locals);
    strcpy_s(va_arg(args,char*),512,input->text);
    *va_arg(args,int*)=input->id;*va_arg(args,int*)=input->value;va_end(args);
    if(offset) *offset+=7;return true;
}
static bool OldConsole(COMMAND_ARGS) { ++originalCalls;FakeGame::Check(*opcodeOffsetPtr==11,"console fallback restores parser offset");return true; }
static BSFaceGenAnimationData* __fastcall GetFace(void* self,void*) {
    FakeGame::NoteStackDepth();
    if(self!=actor) ++badSelf;
    return face;
}
static void Tick(float dt);
#include "legacy_tests.inc"
static TESForm* __cdecl Lookup(UInt32 id) { FakeGame::NoteStackDepth(); ++lookupCalls; return id==0x14?actor:Fake::Find(id); }
static void Build() {
    for(auto& signature:MfgLE::signatures) {
        FakeGame::Check(FakeGame::EnsurePage(signature.address,32),"signature page mapped");
        memcpy(reinterpret_cast<void*>(signature.address),signature.bytes,signature.length);
    }
    // Original update prologue: sub esp,0xC / push ebx / push ebp / push esi.
    // Stub tail restores exactly those registers and uses the real ret 8 ABI.
    // push original dt / push ECX / call cdecl Merge / clean stack / ret 8.
    unsigned char tail[]={0xFF,0x74,0x24,0x1C,0x51,0xE8,0,0,0,0,0x83,0xC4,0x08,
        0x5E,0x5D,0x5B,0x83,0xC4,0x0C,0xC2,0x08,0x00};
    *reinterpret_cast<int*>(tail+6)=reinterpret_cast<UInt32>(Merge)-(MfgLE::UpdateAddress+6+10);
    memcpy(reinterpret_cast<void*>(MfgLE::UpdateAddress+6),tail,sizeof(tail));
    const unsigned char destroy[]={0x5E,0xC3};
    memcpy(reinterpret_cast<void*>(MfgLE::DestroyAddress+10),destroy,sizeof(destroy));
    FakeGame::WriteStub(0x00451A30,reinterpret_cast<void*>(Lookup));
    FakeGame::WriteStub(0x00514830,reinterpret_cast<void*>(ExtractRealMock));
    // Lookup is already detoured in this fake environment; F7 tolerates this
    // non-hook-site signature difference. Hook prologues themselves remain exact.
    memset(actorBytes,0,sizeof(actorBytes)); memset(faceBytes,0,sizeof(faceBytes));
    vtable[0x63]=reinterpret_cast<UInt32>(GetFace);
    *reinterpret_cast<UInt32**>(actorBytes)=vtable;
    actor->formID=0x14;actor->formType=kFormType_Character;
    *reinterpret_cast<UInt32*>(actorBytes+0x88)=reinterpret_cast<UInt32>(process);
    process[1]=reinterpret_cast<UInt32>(middle);
    middle[0xE0/4]=reinterpret_cast<UInt32>(node);
    node[0xDC/4]=reinterpret_cast<UInt32>(face);
    face->keyFrames[0].count=17;face->keyFrames[0].values=primaryExpression;
    face->keyFrames[1].count=17;face->keyFrames[1].values=expression;
    face->keyFrames[2].count=17;face->keyFrames[2].values=modifier;
    face->keyFrames[3].count=16;face->keyFrames[3].values=phoneme;
    face->keyFrames[5].count=17;face->keyFrames[5].values=renderedExpression;
    face->keyFrames[7].count=17;face->keyFrames[7].values=renderedModifier;
    face->keyFrames[9].count=16;face->keyFrames[9].values=renderedPhoneme;
    Fake::Build();
    auto command=reinterpret_cast<CommandInfo*>(0x0124D4D0);
    command->longName="ModifyFaceGen";command->execute=OldConsole;
    messageAPI.RegisterListener=ListenMock;
    SKSEInterface skse={};skse.runtimeVersion=RUNTIME_VERSION_1_9_32_0;
    skse.QueryInterface=InterfaceMock;skse.GetPluginHandle=HandleMock;
    PluginInfo info={};
    FakeGame::Check(SKSEPlugin_Query(&skse,&info),"LE query accepted");
    skse.runtimeVersion=0x01064970;FakeGame::Check(!SKSEPlugin_Query(&skse,&info),"SE query rejected");
    skse.runtimeVersion=RUNTIME_VERSION_1_9_32_0;skse.isEditor=1;
    FakeGame::Check(!SKSEPlugin_Query(&skse,&info),"editor rejected");skse.isEditor=0;
    FakeGame::Check(SKSEPlugin_Load(&skse),"real plugin load and hook installation on fake SKSE");
    FakeGame::Check(registration && listener,"Papyrus and lifecycle callbacks registered");
}
static void Tick(float dt) {
    using Fn=bool (__thiscall*)(BSFaceGenAnimationData*,float,bool);
    const bool result=reinterpret_cast<Fn>(MfgLE::UpdateAddress)(face,dt,true);
    FakeGame::Check(result==lastMerge,"update ABI returns native dirty result");
}
static void Tests() {
    using FakeGame::Check;
    MfgLE::Request request={};request.formID=0x14;request.mode=1;request.id=2;request.value=100;request.speed=1;
    MfgLE::ApplyRequest(request);
    Check(modifier[2]==0,"smooth request does not jump to target");
    Tick(0.25f);Check(FakeGame::Near(modifier[2],0.25f,0.0001f),"quarter-second interpolation");
    Tick(0.25f);Tick(0.25f);Tick(0.25f);
    Check(modifier[2]==1,"transition reaches exact target");
    Check(MfgLE::GetValue(actor,1,2)==100,"getter reads actor through real virtual slot");
    request.mode=-3;MfgLE::ApplyRequest(request);Tick(0.25f);
    Check(FakeGame::Near(modifier[2],0.75f,0.0001f),"modifier reset fades instead of snapping");
    Tick(0.75f);Check(modifier[2]==0,"reset finishes at exact zero");
    Check(renderedModifier[2]==0,"final modifier cache resets to zero");
    request.mode=2;request.id=10;request.value=100;MfgLE::ApplyRequest(request);Tick(0.5f);
    Check(FakeGame::Near(expression[10],0.5f,0.0001f),"expression interpolation");
    Check(renderedExpression[10]==expression[10] && renderedExpression[10]>0,"manual expression reaches final rendered array");
    Check(primaryExpression[10]==0,"manual expression leaves engine primary source intact");
    Check(face->overrideFlag==1,"expression override enabled");
    request.mode=-1;MfgLE::ApplyRequest(request);Tick(1);
    Check(expression[10]==0 && face->overrideFlag==0,"reset releases expression override");
    Check(renderedExpression[10]==0,"final expression cache resets to zero");
    primaryExpression[8]=0.8f;
    Check(MfgLE::GetValue(actor,3,0)==8 && MfgLE::GetValue(actor,2,8)==80,"getter preserves natural engine expression when no override");
    primaryExpression[8]=0;
    request.mode=0;request.id=5;request.value=200;request.speed=0;MfgLE::ApplyRequest(request);
    Check(phoneme[5]==2,"immediate 200 percent phoneme");
    Tick(0.1f);Check(renderedPhoneme[5]==2,"immediate phoneme reaches rendered array");
    request.value=0;MfgLE::ApplyRequest(request);Tick(0);Tick(0.1f);
    Check(renderedPhoneme[5]==0,"immediate reset survives a zero-delta native update");
    request.value=200;MfgLE::ApplyRequest(request);Tick(0.1f);
    request.value=100;request.speed=1;MfgLE::ApplyRequest(request);
    face->unk1A2=1;Tick(0.5f);Check(phoneme[5]==2,"frozen actors are not interpolated");
    face->unk1A2=0;Tick(0.5f);Check(phoneme[5]==1.5f,"transition resumes after frozen flag clears");
    MfgLE::ClearState();Tick(1);Check(phoneme[5]==1.5f,"load reset clears pending transition");
    request.value=0;MfgLE::ApplyRequest(request);Tick(1);
    Check(phoneme[5]==1.5f,"queued pre-load request rejected after generation changes");
    request.generation=MfgLE::generation;MfgLE::ApplyRequest(request);
    using Dtor=void (__thiscall*)(BSFaceGenAnimationData*);
    reinterpret_cast<Dtor>(MfgLE::DestroyAddress)(face);Tick(1);
    Check(phoneme[5]==1.5f,"destruction prevents stale pointer transition reuse");
    request.preset=true;request.openMouth=true;request.speed=0;request.values[30]=2;request.values[31]=1;
    request.values[18]=0.8f;MfgLE::ApplyRequest(request);
    Check(phoneme[5]==1.5f && modifier[2]==0.8f && expression[2]==1,"preset open-mouth flag and expression/modifier channels");
    auto old=phoneme[5];face->keyFrames[3].count=15;
    request.preset=false;request.mode=0;request.id=5;request.value=0;MfgLE::ApplyRequest(request);
    Check(phoneme[5]==old,"invalid source count rejects write");face->keyFrames[3].count=16;
    request.formID=0x123;MfgLE::ApplyRequest(request);Check(phoneme[5]==old,"missing actor safely rejected");
    Check(MfgLE::GetValue(nullptr,0,0)==-1,"null actor getter");
    Check(!MfgLE::Readable(reinterpret_cast<void*>(0x0DA4F3CE),4),"misaligned pointer rejected");
    Check(!MfgLE::Readable(reinterpret_cast<void*>(0xFFFF0000),4),"upper guard rejected");
    Check(badSelf==0 && lookupCalls>0,"read-only chain avoids mutating face getter and resolves form");
    auto oldMiddle=process[1];process[1]=0;
    Check(MfgLE::GetValue(actor,0,5)==-1,"broken middle link rejected");process[1]=oldMiddle;
    auto oldNode=middle[0xE0/4];middle[0xE0/4]=0x0DA4F3CE;
    Check(MfgLE::GetValue(actor,0,5)==-1,"misaligned face node rejected");middle[0xE0/4]=oldNode;
    auto oldFace=node[0xDC/4];node[0xDC/4]=0;
    Check(MfgLE::GetValue(actor,0,5)==-1,"missing face data rejected");node[0xDC/4]=oldFace;
    float parsed=0;
    Check(MfgLE::Decimal("0,75",parsed) && parsed==0.75f,"comma locale config");
    Check(MfgLE::Decimal("0.75",parsed) && parsed==0.75f,"dot locale config");
    Check(!MfgLE::Decimal("nan",parsed) && !MfgLE::Decimal("0.5junk",parsed),"invalid decimals rejected");
    Check(MfgLE::Clamp(std::numeric_limits<float>::quiet_NaN(),0,2)==0,"nonfinite morph rejected");
    Check(!MfgLE::ValidRequest(1,14) && !MfgLE::ValidRequest(0,-1) && MfgLE::ValidRequest(2,0),"API bounds include mood zero");
    Check(MfgLE::Step(0,1,0,0)==1 && MfgLE::Step(0,1,-1,1)==0,"zero speed and negative delta");
    Check(!Smooth(nullptr,actor,1,14,100,1),"native rejects invalid modifier id");
    Check(!Smooth(nullptr,actor,0,1,100,std::numeric_limits<float>::quiet_NaN()),"native rejects nonfinite speed");
    Check(!Smooth(nullptr,nullptr,0,1,100,1),"native rejects missing actor");
    Check(!Reset(nullptr,actor,2,1),"native rejects invalid reset mode");
    Check(Smooth(nullptr,actor,0,4,100,0),"Papyrus setter queues main-thread task");
    Check(phoneme[4]==0 && pending,"setter does not mutate engine on Papyrus call");
    Drain();Check(phoneme[4]==1,"main-thread delegate applies queued command");
    Check(Get(nullptr,actor,0,4)==100,"Papyrus getter dispatch");
    Check(Smooth(nullptr,actor,0,4,0,0),"second setter queued");
    SKSEMessagingInterface::Message message={};message.type=SKSEMessagingInterface::kMessage_PreLoadGame;
    listener(&message);Drain();Check(phoneme[4]==1,"lifecycle discards stale queued task");
    ConsoleInput console={"ngsmile",-1,0};UInt32 offset=11;double result=0;
    auto handler=reinterpret_cast<CommandInfo*>(0x0124D4D0)->execute;
    Check(handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset) && result==1,"real console hook queues smile");
    Drain();Tick(0.75f);Tick(0.75f);Check(expression[10]==1,"console test reaches happy expression");
    Check(renderedExpression[10]==1,"console smile reaches final morph");
    for(const char* status : {"ngstatus","status","statut"}) {
        console.text=status;offset=11;
        Check(handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset) && result==1 && originalCalls==0,"status aliases handled by plugin");
    }
    console.text="custom";offset=11;
    Check(handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset) && originalCalls==1,"unhandled command preserved");
    console.text="ngreset";offset=11;handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset);Drain();Tick(0.75f);Tick(0.75f);
    // 1.0.12: with MfgConsole.dll loaded, the plain verbs go back to it (its console.swf
    // table and its live edits), the ng verbs and Papyrus stay here.
    const int callsBefore=originalCalls;
    const unsigned queuedBefore=queued;
    legacyConsoleOverride=1;
    for(const char* verb : {"phoneme","modifier","expression","reset"}) {
        console.text=verb;console.id=4;console.value=50;offset=11;
        Check(handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset),"plain verb accepted with MfgConsole loaded");
    }
    Check(originalCalls==callsBefore+4,"plain mfg verbs handed to MfgConsole.dll when it is loaded");
    Check(queued==queuedBefore,"plain mfg verbs are not also queued by this plugin");
    console.text="ngmouth";console.id=-1;console.value=0;offset=11;
    handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset);
    Check(originalCalls==callsBefore+4 && result==1,"ng verbs stay with this plugin with MfgConsole loaded");
    passToMfgConsole=false;console.text="phoneme";console.id=4;console.value=50;offset=11;
    handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset);
    Check(originalCalls==callsBefore+4,"bPassToMfgConsole=0 keeps the plain verbs here");
    passToMfgConsole=true;legacyConsoleOverride=0;
    console.text="ngreset";console.id=-1;console.value=0;offset=11;handler(nullptr,&console,actor,nullptr,nullptr,nullptr,&result,&offset);Drain();Tick(0.75f);Tick(0.75f);
    Check(expression[10]==0 && face->overrideFlag==0,"console reset releases expression");
    Check(renderedExpression[10]==0 && renderedModifier[2]==0 && renderedPhoneme[4]==0,"console reset clears final morph caches");
    Check(queued>=4,"queued commands exercised without raw actor capture");
    request.generation=MfgLE::generation;
    LARGE_INTEGER start,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    for(int i=0;i<2000;++i) { request.formID=0x14;request.mode=1;request.id=2;request.speed=1;request.value=i%2?100:0;MfgLE::ApplyRequest(request);Tick(0.05f); }
    QueryPerformanceCounter(&end);
    printf("request+tick bench: %llu ns/pair; state storage fixed at %u slots; no tick heap allocation\n",
        static_cast<unsigned long long>((end.QuadPart-start.QuadPart)*1000000000LL/frequency.QuadPart/2000),MfgLE::Slots);
    Check(!MfgLE::disabled,"2000 real-code ticks without faults");
    LegacyCoreTests();
    LegacyEngineTests();
    auto lock=reinterpret_cast<UInt32*>(faceBytes+0x1A8);
    Check(lock[0]==0 && lock[1]==0,"recursive engine lock fully released");
}
int main() { return FakeGame::Run(Build,Tests); }
