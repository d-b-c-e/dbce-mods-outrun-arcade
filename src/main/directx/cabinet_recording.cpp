#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "cabinet_recording.hpp"
#include "../../../lib/toolkit/session/session_writer.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>

namespace cabinet_recording {
#ifdef CABINET_RECORDING_TEST_CLOCK
std::chrono::steady_clock::time_point test_now() noexcept;
#endif
namespace {
using Clock=std::chrono::steady_clock;
Clock::time_point clock_now() noexcept {
#ifdef CABINET_RECORDING_TEST_CLOCK
    return test_now();
#else
    return Clock::now();
#endif
}
cabinet_signal::Buffer buffer;
cabinet_signal::Frame row;
bool configured=false,started=false,pending=false,saved=false;
unsigned requests=0,deliveries=0,seconds=0;
Clock::time_point first;
std::filesystem::path directory;
std::string lease,armed_utc;
const char* failure="";
Clock::time_point next_lease_check;
bool hex(const char* text,size_t count) {
    return text && std::strlen(text)==count && std::strspn(text,"0123456789abcdef")==count;
}
bool same_lease() {
    const wchar_t* local=_wgetenv(L"LOCALAPPDATA");
    if(!local || lease.empty()) return false;
    std::ifstream file(std::filesystem::path(local)/L"dbce"/L"test-slot.txt",std::ios::binary);
    std::string line;
    if(!std::getline(file,line) || line.size()>4096) return false;
    if(!line.empty() && line.back()=='\r') line.pop_back();
    return line==lease;
}
void fail(const char* reason) noexcept {
    failure=reason;buffer.stop();pending=false;
}
const char* reason() {
    if(*failure) return failure;
    switch(buffer.reason()) {
        case cabinet_signal::End::Duration:return "duration";
        case cabinet_signal::End::Overflow:return "overflow";
        case cabinet_signal::End::Invalid:return "invalid";
        case cabinet_signal::End::Discontinuity:return "discontinuity";
        default:return "interrupted";
    }
}
void save() {
    if(saved || !configured || directory.empty()) return;
    if(!same_lease())fail("lease lost before seal");
    saved=true; // saving is single-shot; no append/retry into an existing result
    dbce::session::Metadata m;
    m.game="CannonBall-SE";m.pluginVersion="cabinet-signal@1";m.toolkitVersion="session-85147b8";
    m.properties={{"contract","cannonball.cabinet-signal@1"},{"model","cabinet-command@2"},
        {"producerSha256",DBCE_CABINET_PRODUCER_SHA256},{"mapperSha256",DBCE_CABINET_MAPPER_SHA256},
        {"durationSeconds",std::to_string(seconds)},{"armedUtc",armed_utc},
        {"physicalOutput","false"},{"telemetryDelivery","false"},
        {"admission","original producer command; nominal mapper calculation only"},
        {"clock","steady_clock microseconds; existing motor update"}};
#ifdef CABINET_RECORDING_TEST_CLOCK
    m.properties["captureKind"]="synthetic producer fixture";
#else
    m.properties["captureKind"]="original gameplay observation";
#endif
    m.channelUnits={{"update","game update index"},{"force.nominal","nominal constant request, -10000..10000"},
        {"input.carIncrement","original 16.16 game increment; not SI speed"}};
    dbce::session::Writer writer;
    const auto temporary=directory/L"source.jsonl.tmp";
    if(!writer.open(temporary.wstring(),m,{64*1024*1024,256*1024,double(seconds)+2})) throw std::runtime_error("open recording");
    const auto& settings=buffer.settings();
    for(const auto& r:buffer.frames()) {
        dbce::session::Channels c={{"update",r.update},{"force.command",r.command},{"force.step",r.step},
            {"force.nominal",r.nominal},{"tuning.maximum",settings.maximum},{"tuning.minimum",settings.minimum},
            {"tuning.holdMs",settings.hold_ms},{"delivery.muted",1},{"delivery.result",r.delivery_result}};
        for(size_t n=0;n<r.inputs.size();++n)c[std::string("input.")+cabinet_signal::input_names[n]]=double(r.inputs[n]);
        for(size_t n=0;n<r.before.size();++n){c[std::string("before.")+cabinet_signal::state_names[n]]=r.before[n];c[std::string("after.")+cabinet_signal::state_names[n]]=r.after[n];}
        if(!writer.sample(double(r.elapsed_us)/1000000,c)) throw std::runtime_error("write sample");
    }
    const bool complete=buffer.complete() && !*failure && !buffer.frames().empty();
    if(complete){if(!writer.stop())throw std::runtime_error("recording flush");}
    else writer.abandon(); // no completed footer for a partial capture
    const auto destination=directory/L"source.jsonl";
    if(!MoveFileW(temporary.c_str(),destination.c_str())) throw std::runtime_error("publish recording");
    const auto outcome=directory/L"outcome.txt";
    HANDLE file=CreateFileW(outcome.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("open outcome");
    std::string text="schema=cannonball.cabinet-signal@1\ncompleted="+std::string(complete?"true":"false")+
        "\nreason="+reason()+"\nrows="+std::to_string(buffer.frames().size())+"\nphysicalOutput=false\ntelemetryDelivery=false\n";
    DWORD written=0;bool ok=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr) && written==text.size();
    ok=FlushFileBuffers(file) && ok;CloseHandle(file);
    if(!ok)throw std::runtime_error("write outcome");
    std::fprintf(stderr,"Cabinet capture: %s rows=%zu reason=%s\n",complete?"complete":"incomplete",buffer.frames().size(),reason());
}
}
void initialize(bool haptic,bool real_cabinet,forcefeedback::CabinetSettings settings) noexcept {
    if(!requested())return;
    try {
        const char* id=std::getenv("DBCE_CANNONBALL_CAPTURE");
        const char* duration=std::getenv("DBCE_CANNONBALL_CAPTURE_SECONDS");
        const char* token=std::getenv("DBCE_CANNONBALL_LEASE");
        const wchar_t* local=_wgetenv(L"LOCALAPPDATA");
        if(!hex(id,32) || !duration || !*duration || std::strspn(duration,"0123456789")!=std::strlen(duration) || std::strlen(duration)>3 ||
           !token || !*token || std::strlen(token)>4096 || !local || !haptic || real_cabinet || configured)
            throw std::runtime_error("invalid request or producer mode");
        seconds=unsigned(std::strtoul(duration,nullptr,10));lease=token;
        if(!same_lease() || !buffer.arm(settings,seconds))throw std::runtime_error("lease/settings/duration refused");
        directory=std::filesystem::path(local)/L"Dbce"/L"StagePlayback"/L"cannonball-force"/id;
        if(std::filesystem::exists(directory))throw std::runtime_error("result already exists");
        std::filesystem::create_directories(directory);configured=true;
        SYSTEMTIME utc;GetSystemTime(&utc);char date[48];
        std::snprintf(date,sizeof(date),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds);
        armed_utc=date;next_lease_check=clock_now();
        std::fprintf(stderr,"Cabinet capture: armed %s; process outputs muted; waiting for offline driving\n",id);
    }catch(const std::exception& e){failure="initialization refused";buffer.stop();std::fprintf(stderr,"Cabinet capture refused: %s; process outputs remain muted\n",e.what());}
}
void service(bool driving,bool paused) noexcept {
    if(!configured || saved)return;
    try {
        // Called from the outer frame, never from the producer's before/after
        // interval. Lease loss and game/menu transitions cannot seal completion.
        const auto now=clock_now();
        if(now>=next_lease_check || !buffer.active()) {
            if(!same_lease())fail("lease lost");
            next_lease_check=now+std::chrono::milliseconds(250);
        }
        if(started && (!driving || paused))fail("driving interrupted");
        if(!buffer.active())save();
    }catch(const std::exception& e){fail("service/save failure");std::fprintf(stderr,"Cabinet capture failed: %s\n",e.what());}
}
void begin(uint32_t update,const cabinet_signal::Inputs& input,const cabinet_signal::State& before) noexcept {
    if(!configured || !buffer.active())return;
    if(pending){fail("nested producer");return;}
    if(input[cabinet_signal::Game]!=12){if(started)fail("game state changed");return;}
    const auto now=clock_now();
    if(!started){started=true;first=now;}
    row={};row.update=update;row.inputs=input;row.before=before;
    row.elapsed_us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(now-first).count());
    pending=true;requests=0;deliveries=0;
}
void request(int command,int step) noexcept {
    if(!pending)return;
    ++requests;row.command=command;row.step=step;
}
void delivery(int result) noexcept {
    if(!pending)return;
    if(requests!=1 || deliveries!=0){fail("delivery order/count invalid");return;}
    ++deliveries;row.delivery_result=result;
    if(result!=-1)fail("force call was not refused by mute guard");
}
void end(const cabinet_signal::State& after) noexcept {
    if(!pending)return;
    pending=false;row.after=after;
    if(requests!=1 || deliveries!=1 || !forcefeedback::cabinet_force(row.command,row.step,buffer.settings(),row.nominal)){
        fail("producer request count or mapper invalid");return;
    }
    buffer.append(row);
}
void close() noexcept {
    if(!configured || saved)return;
    if(buffer.active() || pending)fail("process exit");
    try {save();}catch(const std::exception& e){std::fprintf(stderr,"Cabinet capture close failed: %s\n",e.what());}
}
}
