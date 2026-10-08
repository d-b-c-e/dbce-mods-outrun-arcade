#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>
#include "../src/main/directx/cabinet_recording.hpp"
#define CABINET_RECORDING_REAL
#include "cabinet_producer_fixture.hpp"
namespace cabinet_recording {
static std::chrono::steady_clock::time_point fake_now{};
std::chrono::steady_clock::time_point test_now() noexcept { return fake_now; }
}
static std::string read(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
int main(int argc,char** argv) {
    CHECK(argc==2);const std::string test=argv[1];
    const auto root=std::filesystem::current_path()/("capture-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    CHECK(!std::filesystem::exists(root));std::filesystem::create_directories(root/"dbce");
    _putenv_s("LOCALAPPDATA",root.string().c_str());
    _putenv_s("DBCE_CANNONBALL_CAPTURE","0123456789abcdef0123456789abcdef");
    _putenv_s("DBCE_CANNONBALL_CAPTURE_SECONDS","1");
    _putenv_s("DBCE_CANNONBALL_LEASE","fixture exact lease");
    {std::ofstream f(root/"dbce"/"test-slot.txt");f<<"fixture exact lease\n";}
    const auto result=root/"Dbce"/"StagePlayback"/"cannonball-force"/"0123456789abcdef0123456789abcdef";
    if(test=="bad-id")_putenv_s("DBCE_CANNONBALL_CAPTURE","../bad");
    if(test=="bad-duration")_putenv_s("DBCE_CANNONBALL_CAPTURE_SECONDS","121");
    if(test=="existing"){
        std::filesystem::create_directories(result);std::ofstream f(result/"sentinel");f<<"original";
    }
    cabinet_recording::initialize(test!="haptic-off",test=="cabinet-mode",{9000,8500,20});
    CHECK(cabinet_recording::requested());
    if(test=="bad-id" || test=="bad-duration" || test=="existing" || test=="haptic-off" || test=="cabinet-mode"){
        cabinet_recording::close();CHECK(!std::filesystem::exists(result/"outcome.txt"));
        if(test=="existing")CHECK(read(result/"sentinel")=="original");
    }else{
        OOutputs m;m.init();Input i{12,0,0,30,90,170,0,180u<<16,0};
        for(uint32_t n=0;n<8;++n){
            cabinet_recording::begin(500+n,{i.game,i.crash,i.skid,i.increment,i.curve,i.wheels,i.x_diff,i.steering,i.motor},snapshot(m));
            if(test=="nested" && n==1)cabinet_recording::begin(501,{},snapshot(m));
            if(test=="unexpected-delivery" && n==1)forcefeedback::delivery_result=0;
            if(test=="early-delivery" && n==1)cabinet_recording::delivery(-1);
            advance(m,i);cabinet_recording::end(snapshot(m));
            if(test=="interrupted" && n==1){cabinet_recording::service(false,true);break;}
            if(test=="lost-lease" && n==7){std::ofstream f(root/"dbce"/"test-slot.txt");f<<"replacement\n";}
            if(test=="write-failure" && n==7){std::ofstream f(result/"source.jsonl.tmp");f<<"original temporary file";}
            // For loss at sealing, intentionally defer service until close.
            if(test!="lost-lease")cabinet_recording::service(true,false);
            cabinet_recording::fake_now+=std::chrono::milliseconds(143);
        }
        cabinet_recording::close();
        const auto outcome=read(result/"outcome.txt"),data=read(result/"source.jsonl");
        if(test=="complete"){
            CHECK(outcome.find("completed=true")!=std::string::npos);
            CHECK(data.find("\"schema\":\"dbce.wheel.session\"")!=std::string::npos);
            CHECK(data.find("\"completed\":true")!=std::string::npos);
            CHECK(data.find("\"writtenSamples\":8")!=std::string::npos);
        }else if(test=="write-failure"){
            CHECK(outcome.empty());CHECK(read(result/"source.jsonl.tmp")=="original temporary file");
        }else{
            CHECK(outcome.find("completed=false")!=std::string::npos);
            CHECK(data.find("\"completed\":true")==std::string::npos);
        }
    }
    CHECK(forcefeedback::requests.size()<=1); // fake only; no device library linked
    std::printf("PASS %s %u checks; isolated files at %s\n",test.c_str(),checks,root.string().c_str());
}
