// Test injection for the profile wheel (see test_inject.hpp). The grammar, the table of running samples and their
// delivery are the toolkit's (vendor/controls/dbce_inject*.hpp), shared with the DirectInput proxies, F-Zero and
// OutRun 2006; this file holds only CannonBall's side: the arming conditions, the session and command file, and the
// SDL <-> DirectInput sample conversion.
#include "sdl2/test_inject.hpp"

#include "vendor/controls/dbce_inject_table.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace ctl = dbce::controls;

namespace test_inject
{
namespace
{
constexpr size_t kMaxFileBytes = 4096;
constexpr int kMaxFileCommands = 32;
constexpr int kMaxSessionCommands = 2000;
constexpr uint64_t kMaxSessionSeconds = 3600;

std::atomic<int> g_force{0};       // 0 undecided, 1 no force (a test was requested), 2 force output started
std::atomic<bool> g_armed{false};
std::mutex g_session;              // the instance, nonce and counts; the table has its own lock
std::string g_instance;            // the profile wheel's DirectInput instance (normalized), as raw commands name it
std::string g_nonce;
std::atomic<uint64_t> g_expires_ms{0};   // on now_ms()'s clock; 0 = no expiry (tests)
int g_session_commands = 0;
ctl::InjectionTable g_table;
bool g_testing = false;
uint64_t g_test_clock = 0, g_next_poll = 0;
#ifdef _WIN32
std::wstring g_commands;
FILETIME g_last_write{};
#endif

void log(const std::string& line) { std::fprintf(stderr, "[test-inject] %s\n", line.c_str()); }

uint64_t now_ms()
{
#ifdef _WIN32
    return g_testing ? g_test_clock : (uint64_t)GetTickCount64();
#else
    return g_test_clock;
#endif
}

bool session_live()
{
    if (!g_armed) return false;
    const uint64_t expires = g_expires_ms;
    if (expires && now_ms() >= expires) {
        g_armed = false;
        const int n = g_table.clearFor("");
        log("test session expired; " + std::to_string(n) + " running sample(s) dropped; injection off");
        return false;
    }
    return true;
}

// inject.on: "nonce=<8-64 letters/digits>" and "expires=<unix seconds>" in (now, now + 1 h].
bool parse_session(const std::string& text, uint64_t now_unix, std::string& nonce, uint64_t& expires_unix, std::string& why)
{
    nonce.clear(); expires_unix = 0;
    if (text.size() > 512) { why = "inject.on is larger than 512 bytes"; return false; }
    std::istringstream in(text);
    std::string line;
    bool have_expiry = false;
    while (std::getline(in, line)) {
        const std::string t = ctl::trim(line);
        if (t.rfind("nonce=", 0) == 0) nonce = t.substr(6);
        else if (t.rfind("expires=", 0) == 0) {
            const std::string v = t.substr(8);
            if (v.empty() || v.size() > 12 || v.find_first_not_of("0123456789") != std::string::npos) { why = "expires= is not unix seconds"; return false; }
            expires_unix = std::strtoull(v.c_str(), nullptr, 10);
            have_expiry = true;
        }
    }
    if (nonce.size() < 8 || nonce.size() > 64) { why = "inject.on names no nonce= of 8-64 letters/digits"; return false; }
    for (char c : nonce)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) { why = "the nonce is not letters/digits"; return false; }
    if (!have_expiry) { why = "inject.on names no expires="; return false; }
    if (expires_unix <= now_unix) { why = "the test session in inject.on has expired"; return false; }
    if (expires_unix - now_unix > kMaxSessionSeconds) { why = "the test session in inject.on ends more than an hour from now"; return false; }
    return true;
}

bool add_line(const std::string& line, std::string& why)
{
    if (!session_live()) { why = "no test session"; return false; }
    std::lock_guard<std::mutex> g(g_session);
    if (g_session_commands >= kMaxSessionCommands) { why = "the session's 2000 commands are used"; return false; }
    ctl::InjectCommand c;
    if (!ctl::parseInject(line, nullptr, c, why)) return false;   // raw only: CannonBall keeps no profile request
    if (c.binding.dev != g_instance) { why = "dev is not the profile wheel (controls.pad_device instance)"; return false; }
    why = g_table.add(c, now_ms());
    if (!why.empty()) return false;
    ++g_session_commands;
    return true;
}

int take_file(const std::string& text, std::string& why)
{
    why.clear();
    if (text.size() > kMaxFileBytes) { why = "inject.txt is larger than 4096 bytes; nothing read"; return 0; }
    std::istringstream in(text);
    std::string line, nonce;
    { std::lock_guard<std::mutex> g(g_session); nonce = g_nonce; }
    bool first = true;
    int commands = 0, accepted = 0;
    while (std::getline(in, line)) {
        const std::string t = ctl::trim(line);
        if (t.empty()) continue;
        if (first) {
            first = false;
            if (t != "nonce=" + nonce) { why = "inject.txt does not name this session's nonce; nothing read"; return 0; }
            continue;
        }
        if (++commands > kMaxFileCommands) { why = "inject.txt has more than 32 commands; the rest were not read"; break; }
        std::string w;
        if (add_line(t, w)) { ++accepted; log("accepted: " + t); }
        else log("refused \"" + t + "\": " + w);
    }
    if (first) why = "inject.txt is empty";
    return accepted;
}

bool arm_with(const std::string& instance, std::string& why)
{
    const std::string normalized = ctl::detail::guid(instance);
    if (normalized.empty()) { why = "controls.pad_device names no DirectInput instance (written by Wheelkit)"; return false; }
    g_table.clearFor("");   // a new arming starts with nothing running
    std::lock_guard<std::mutex> g(g_session);
    g_instance = normalized;
    g_session_commands = 0;
    g_armed = true;
    return true;
}

unsigned long hat_angle(uint8_t bits)   // SDL hat bits (up 1, right 2, down 4, left 8) -> DirectInput POV
{
    switch (bits) {
    case 1: return 0; case 3: return 4500; case 2: return 9000; case 6: return 13500;
    case 4: return 18000; case 12: return 22500; case 8: return 27000; case 9: return 31500;
    default: return 0xFFFFFFFFul;
    }
}

uint8_t hat_bits(unsigned long angle)
{
    if ((angle & 0xFFFF) == 0xFFFF || angle >= 36000) return 0;
    static const uint8_t bits[8] = {1, 3, 2, 6, 4, 12, 8, 9};
    return bits[((angle + 2250) / 4500) % 8];
}
} // namespace

bool latch_no_force() { int undecided = 0; return g_force.compare_exchange_strong(undecided, 1) || undecided == 1; }
bool allow_force_output() { int undecided = 0; return g_force.compare_exchange_strong(undecided, 2) || undecided == 2; }
bool no_force() { return g_force == 1; }

std::string identity_error(const std::vector<DiDevice>& devices, unsigned vendor, unsigned product, const std::string& instance)
{
    const std::string want = ctl::detail::guid(instance);
    if (want.empty()) return "controls.pad_device names no DirectInput instance (written by Wheelkit)";
    int matches = 0;
    bool found = false;
    for (const auto& d : devices)
        if (d.vendor == vendor && d.product == product) { ++matches; found = found || ctl::detail::guid(d.instance) == want; }
    if (matches == 0) return "no attached DirectInput game controller has the profile wheel's vendor/product";
    if (matches > 1) return "two attached DirectInput game controllers share the profile wheel's vendor/product";
    if (!found) return "the attached profile wheel is another DirectInput instance than the profile's";
    return {};
}

#ifdef _WIN32
namespace
{
BOOL CALLBACK collect_device(const DIDEVICEINSTANCEW* d, void* ctx)
{
    char text[40];
    const GUID& g = d->guidInstance;
    std::snprintf(text, sizeof text, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", g.Data1, g.Data2, g.Data3,
                  g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    // guidProduct.Data1 is MAKELONG(vendor, product) for HID game controllers.
    static_cast<std::vector<DiDevice>*>(ctx)->push_back({text, (unsigned)(d->guidProduct.Data1 & 0xFFFF), (unsigned)(d->guidProduct.Data1 >> 16)});
    return DIENUM_CONTINUE;
}
// Listing only: no device is created, acquired or given any effect.
bool list_devices(std::vector<DiDevice>& out, std::string& why)
{
    IDirectInput8W* di = nullptr;
    if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&di, nullptr)) || !di)
    { why = "DirectInput is unavailable for the identity check"; return false; }
    const HRESULT hr = di->EnumDevices(DI8DEVCLASS_GAMECTRL, collect_device, &out, DIEDFL_ATTACHEDONLY);
    di->Release();
    if (FAILED(hr)) { why = "DirectInput could not list the attached game controllers"; return false; }
    return true;
}
}
#endif

std::vector<std::string> init(const std::string& instance, bool wanted, unsigned vendor, unsigned product)
{
    std::vector<std::string> out;
    g_armed = false;
#ifdef _WIN32
    const wchar_t* base = _wgetenv(L"LOCALAPPDATA");
    if (!base) return out;
    const std::wstring dir = std::wstring(base) + L"\\dbce\\outrun-arcade\\";
    const std::wstring on = dir + L"inject.on";
    if (GetFileAttributesW(on.c_str()) == INVALID_FILE_ATTRIBUTES) return out;   // not requested
    auto refuse = [&](const std::string& why) { out.push_back("[test-inject] test injection refused: " + why); return out; };
    // The request alone rules force out for the whole process, before anything below can refuse it.
    if (!latch_no_force()) return refuse("force output already started in this process");
    out.push_back("[test-inject] test injection requested (inject.on): no force output in this process");
    // DBCE_FFB_MUTE is the second, independent interlock: the force library never loads, no SDL haptic opens.
    const char* muted = std::getenv("DBCE_FFB_MUTE");
    if (!muted || !std::strcmp(muted, "0"))
        return refuse("the process is not force-muted (start it with DBCE_FFB_MUTE=1, which keeps the force library unloaded)");
    std::string text;
    {
        std::ifstream f(on, std::ios::binary);
        char buf[513];
        f.read(buf, sizeof buf);
        text.assign(buf, (size_t)f.gcount());
    }
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const uint64_t now_unix = ((((uint64_t)ft.dwHighDateTime) << 32 | ft.dwLowDateTime) / 10000000ull) - 11644473600ull;
    std::string nonce, why;
    uint64_t expires_unix = 0;
    if (!parse_session(text, now_unix, nonce, expires_unix, why)) return refuse(why);
    if (!wanted) return refuse("config.xml names no profile wheel (controls.pad_device)");
    std::vector<DiDevice> devices;
    if (!list_devices(devices, why)) return refuse(why);
    const std::string identity = identity_error(devices, vendor, product, instance);
    if (!identity.empty()) return refuse(identity);
    if (!arm_with(instance, why)) return refuse(why);
    {
        std::lock_guard<std::mutex> g(g_session);
        g_nonce = nonce;
    }
    g_expires_ms = now_ms() + (expires_unix - now_unix) * 1000ull;
    g_commands = dir + L"inject.txt";
    WIN32_FILE_ATTRIBUTE_DATA a;
    g_last_write = GetFileAttributesExW(g_commands.c_str(), GetFileExInfoStandard, &a) ? a.ftLastWriteTime : FILETIME{};
    out.push_back("[test-inject] test injection ARMED (DBCE_FFB_MUTE: no force output) for " + g_instance +
                  "; session ends in " + std::to_string(expires_unix - now_unix) + " s");
#else
    (void)instance; (void)wanted; (void)vendor; (void)product;
#endif
    return out;
}

bool armed() { return g_armed; }

void poll()
{
#ifdef _WIN32
    if (!session_live() || g_testing || g_commands.empty()) return;
    const uint64_t now = now_ms();
    if (now < g_next_poll) return;
    g_next_poll = now + 100;
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(g_commands.c_str(), GetFileExInfoStandard, &a)) return;
    if (CompareFileTime(&a.ftLastWriteTime, &g_last_write) <= 0) return;
    g_last_write = a.ftLastWriteTime;   // each version is read once, whatever its fate
    std::string text;
    if (a.nFileSizeHigh || a.nFileSizeLow > kMaxFileBytes) text.assign(kMaxFileBytes + 1, ' ');   // refused unread
    else {
        std::ifstream in(g_commands, std::ios::binary);
        char buf[kMaxFileBytes + 1];
        in.read(buf, sizeof buf);
        text.assign(buf, (size_t)in.gcount());
    }
    std::string why;
    take_file(text, why);
    if (!why.empty()) log(why);
#endif
}

int apply(int16_t* axes, int axis_count, uint8_t* buttons, int button_count, uint8_t* hats, int hat_count)
{
    if (!session_live() || !g_table.any()) return 0;
    if (axis_count < 0 || axis_count > 8 || button_count < 0 || button_count > 128 || hat_count < 0 || hat_count > 4) return 0;
    long ax[8];
    unsigned char bt[128] = {0};
    unsigned long pv[4];
    for (int i = 0; i < 8; ++i) ax[i] = i < axis_count && axes ? (long)axes[i] + 32768 : 32768;
    for (int i = 0; i < button_count; ++i) bt[i] = buttons && buttons[i] ? 0x80 : 0;
    for (int i = 0; i < 4; ++i) pv[i] = i < hat_count && hats ? hat_angle(hats[i]) : 0xFFFFFFFFul;
    ctl::StateView v;
    v.axes = axis_count > 0 && axes ? ax : nullptr;
    v.buttons = button_count > 0 && buttons ? bt : nullptr;
    v.buttonCount = button_count;
    v.povs = hat_count > 0 && hats ? pv : nullptr;
    v.povCount = hat_count;
    std::string instance;
    { std::lock_guard<std::mutex> g(g_session); instance = g_instance; }
    std::vector<std::string> lines;
    const int n = g_table.apply(instance, v, now_ms(), [axis_count](int axis, long& mn, long& mx) {
        if (axis >= axis_count) return false;   // the stick has no such axis: never delivered
        mn = 0; mx = 65535;
        return true;
    }, lines);
    for (auto& l : lines) log(l);
    for (int i = 0; i < axis_count && axes; ++i) {
        const long s = ax[i] - 32768;
        axes[i] = (int16_t)(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
    }
    for (int i = 0; i < button_count && buttons; ++i) buttons[i] = bt[i] ? 1 : 0;
    for (int i = 0; i < hat_count && hats; ++i) hats[i] = hat_bits(pv[i]);
    return n;
}

void device_closed()
{
    const int n = g_table.clearFor("");
    if (n) log("the stick closed; " + std::to_string(n) + " running sample(s) dropped");
}

bool test_arm(const std::string& instance)
{
    g_testing = true;
    g_armed = false;
    { std::lock_guard<std::mutex> g(g_session); g_nonce = "test"; }
    g_expires_ms = 0;
    std::string why;
    return arm_with(instance, why);
}
bool test_command(const std::string& line, std::string& why) { return add_line(line, why); }
int test_file(const std::string& text, std::string& why) { return take_file(text, why); }
void test_expire(uint64_t expires_ms) { g_expires_ms = expires_ms; }
bool test_session(const std::string& text, uint64_t now_unix, std::string& nonce_or_why)
{
    std::string nonce, why;
    uint64_t expires = 0;
    const bool ok = parse_session(text, now_unix, nonce, expires, why);
    nonce_or_why = ok ? nonce : why;
    return ok;
}
void test_clock(uint64_t now_ms) { g_test_clock = now_ms; }
}
