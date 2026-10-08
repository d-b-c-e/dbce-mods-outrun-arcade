// Compile the production Windows adapter against fake toolkit/foreground calls.
// No DLL is loaded, no device is enumerated or opened, no OS input is sent.
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static int checks, loads, frees, unloads, opens, zeros, stops, guards, holds, strict;
static int version = 600;
static int last_hr = static_cast<int>(E_FAIL);
static ULONGLONG now_ms = 0;
static bool foreground = true, load_ok = true, init_ok = true, guard_ok = true, send_ok = true;
static GUID selected{};
static std::vector<int> sent;
static int enumerations;
#define CHECK(v) do { ++checks; if (!(v)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v); std::exit(1); } } while(0)
static int fake_init(int hwnd) { ++opens; CHECK(hwnd==0); return init_ok; }
static int fake_send(int x, int y) { CHECK(y==0); sent.push_back(x); return send_ok; }
static int fake_stop() { ++stops; return 1; }
static void fake_zero() { ++zeros; }
static void fake_free() { ++frees; }
static void fake_strict(int value) { strict=value; }
static void fake_guid(const void* value) { std::memcpy(&selected,value,16); }
static void fake_hold(int value) { holds=value; }
static int fake_guards() { ++guards; return guard_ok; }
static int fake_version() { return version; }
static int fake_hr() { return last_hr; }
static int fake_enum() { ++enumerations; return 2; }
static int fake_name(int index,char* out,int size) { strcpy_s(out,size,index==0 ? "Test Wheel" : "No Identity"); return 1; }
static int fake_device_guid(int index,void* out) {
    GUID guid{}; if(index==0) guid.Data1=0x12345678;
    std::memcpy(out,&guid,16); return 1;
}
#define DBCE_WHEELFFB_H
struct WheelFfbApi {
    HMODULE module{};
    int (*InitDirectInput)(int){};
    int (*SetDeviceForcesXY)(int,int){};
    int (*StopEffect)(){};
    void (*ZeroForces)(){};
    void (*FreeDirectInput)(){};
    void (*SetStrictDeviceSelection)(int){};
    void (*SetPreferredDeviceGuid)(const void*){};
    void (*SetHoldTimeoutMs)(int){};
    int (*InstallExitGuards)(){};
    int (*GetWheelFfbVersion)(){};
    int (*GetLastHResult)(){};
    int (*EnumerateDevices)(){};
    int (*GetDeviceName)(int,char*,int){};
    int (*GetDeviceGuid)(int,void*){};
};
static int WheelFfb_LoadBeside(WheelFfbApi* api, HMODULE module, const wchar_t* file) {
    ++loads; CHECK(module==nullptr); CHECK(std::wcscmp(file,L"WheelFfb.dll")==0);
    *api={reinterpret_cast<HMODULE>(1),fake_init,fake_send,fake_stop,fake_zero,fake_free,fake_strict,fake_guid,fake_hold,fake_guards,fake_version,fake_hr,fake_enum,fake_name,fake_device_guid};
    return load_ok;
}
static void WheelFfb_Unload(WheelFfbApi* api) { ++unloads; *api={}; }
static HWND fake_foreground() { return foreground?reinterpret_cast<HWND>(1):nullptr; }
static DWORD fake_pid(HWND, DWORD* id) { *id=GetCurrentProcessId(); return 1; }
static BOOL fake_iconic(HWND) { return FALSE; }
static ULONGLONG fake_clock() { return now_ms; }
#define GetForegroundWindow fake_foreground
#define GetWindowThreadProcessId fake_pid
#define IsIconic fake_iconic
#define GetTickCount64 fake_clock
#include "../src/main/directx/ffeedback_windows.inl"
int main(int argc,char** argv) {
    CHECK(argc==2);
    using namespace forcefeedback;
    GUID guid{};
    CHECK(explicit_guid("{12345678-abcd-9876-5432-123456abcdef}",guid));
    CHECK(guid.Data1==0x12345678 && guid.Data2==0xabcd && guid.Data3==0x9876 && guid.Data4[0]==0x54 && guid.Data4[7]==0xef);
    CHECK(explicit_guid("12345678-ABCD-9876-5432-123456ABCDEF",guid));
    CHECK(!explicit_guid(nullptr,guid)); CHECK(!explicit_guid("wheel name",guid));
    CHECK(!explicit_guid("{12345678-abcd-9876-5432-123456abcdefX",guid));
    CHECK(!explicit_guid("12345678abcd-9876-5432-123456abcdef",guid));
    CHECK(!explicit_guid("12345678-abcd-9876-5432-123456abcdeZ",guid));
    _putenv_s("FF_TARGET_GUID","12345678-abcd-9876-5432-123456abcdef");
    _putenv_s("DBCE_FFB_MUTE","");
    const char* test=argv[1];
    if(std::strcmp(test,"saved-guid")==0) {
        _putenv_s("FF_TARGET_GUID",""); configure_guid("87654321-abcd-9876-5432-123456abcdef");
        CHECK(init(9000,8500,20) && loads==0); set_active(true);
        CHECK(selected.Data1==0x87654321 && opens==1 && strict==1 && sent[0]==0); close();
    } else if(std::strcmp(test,"invalid-override")==0) {
        configure_guid("87654321-abcd-9876-5432-123456abcdef"); _putenv_s("FF_TARGET_GUID","broken");
        CHECK(!init(9000,8500,20) && loads==0 && opens==0);
    } else if(std::strcmp(test,"override")==0) {
        configure_guid("87654321-abcd-9876-5432-123456abcdef");
        CHECK(init(9000,8500,20)); set_active(true); CHECK(selected.Data1==0x12345678); close();
    } else if(std::strcmp(test,"catalog")==0) {
        // Read-only menu discovery is allowed while output is muted, but never
        // invokes Init, selection, effect creation, force setters or exit guards.
        _putenv_s("DBCE_FFB_MUTE","1");
        auto choices=enumerate_wheels();
        CHECK(choices.size()==1 && choices[0].name=="Test Wheel" && choices[0].guid=="12345678-0000-0000-0000-000000000000");
        CHECK(enumerations==1 && loads==1 && opens==0 && sent.empty() && guards==0 && strict==0);
        enumerate_wheels(); CHECK(loads==1 && enumerations==2); close(); CHECK(frees==1 && unloads==1);
    } else if(std::strcmp(test,"retire-choice")==0) {
        CHECK(init(9000,8500,20)); set_active(true); CHECK(set(15,0)==0);
        enumerate_wheels(); CHECK(opens==1 && frees==0); close();
        auto count=sent.size(); set_active(true); CHECK(set(15,0)==-1 && sent.size()==count && opens==1);
    } else if(std::strcmp(test,"capture-mute")==0) {
        _putenv_s("DBCE_CANNONBALL_CAPTURE","invalid request still mutes");
        CHECK(!init(9000,8500,20));_putenv_s("DBCE_CANNONBALL_CAPTURE","");
        set_active(true);CHECK(set(15,0)==-1); // startup mute cannot be lifted later
        CHECK(loads==0 && opens==0 && sent.empty());
    } else if(std::strcmp(test,"mute")==0) {
        _putenv_s("DBCE_FFB_MUTE","1"); CHECK(!init(9000,8500,20)); CHECK(loads==0);
    } else if(std::strcmp(test,"missing-guid")==0) {
        _putenv_s("FF_TARGET_GUID",""); CHECK(!init(9000,8500,20)); CHECK(loads==0);
    } else if(std::strcmp(test,"zero-guid")==0) {
        _putenv_s("FF_TARGET_GUID","00000000-0000-0000-0000-000000000000"); CHECK(!init(9000,8500,20)); CHECK(loads==0);
    } else if(std::strcmp(test,"background")==0) {
        foreground=false; CHECK(init(9000,8500,20)); set_active(true); CHECK(loads==0 && opens==0 && !is_supported());
        foreground=true; set_active(true); CHECK(loads==1 && opens==1 && is_supported()); close();
    } else if(std::strcmp(test,"partial-load")==0) {
        load_ok=false; CHECK(init(9000,8500,20)); set_active(true); CHECK(!is_supported() && opens==0 && frees==1 && unloads==1);
    } else if(std::strcmp(test,"wrong-version")==0) {
        version=901; CHECK(init(9000,8500,20)); set_active(true); CHECK(!is_supported() && opens==0 && frees==1 && unloads==1);
    } else if(std::strcmp(test,"init-failure")==0) {
        init_ok=false; CHECK(init(9000,8500,20)); set_active(true); CHECK(!is_supported() && opens==1 && frees==1 && unloads==1 && guards==0);
    } else if(std::strcmp(test,"guard-failure")==0) {
        guard_ok=false; CHECK(init(9000,8500,20)); set_active(true); CHECK(!is_supported() && opens==1 && frees==1 && unloads==1);
    } else if(std::strcmp(test,"live-fake")==0) {
        CHECK(init(9000,8500,20)); CHECK(opens==0 && sent.empty());
        set_active(true); CHECK(set(1,0)==0 && sent.back()==-9000);
        CHECK(strict==1 && holds==100 && selected.Data1==0x12345678 && guards==1 && sent[0]==0);
        foreground=false; CHECK(set(15,0)==0 && sent.back()==0);
        auto count=sent.size(); CHECK(set(15,0)==0 && sent.size()==count);
        foreground=true; set_active(true); CHECK(set(15,0)==0 && sent.back()==9000 && sent[sent.size()-2]==0);
        send_ok=false; CHECK(set(15,0)==-1); CHECK(!is_supported() && frees==1 && unloads==1 && zeros==1);
        CHECK(!init(9000,8500,20) && opens==1); close(); CHECK(frees==1);
    } else if(std::strcmp(test,"transient")==0) {
        CHECK(init(9000,8500,20)); set_active(true);
        last_hr=static_cast<int>(DIERR_INPUTLOST); send_ok=false;
        CHECK(set(15,0)==-1 && is_supported() && frees==0);
        now_ms=100; send_ok=true;
        CHECK(set(15,0)==0 && is_supported() && opens==1);
        CHECK(sent[sent.size()-2]==0 && sent.back()==9000); close(); CHECK(frees==1);
    } else CHECK(false);
    std::printf("PASS: %d production adapter checks (%s), no hardware\n",checks,test);
}
