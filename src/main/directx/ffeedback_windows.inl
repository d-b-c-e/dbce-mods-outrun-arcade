// Included only on Windows. No gamepad-rumble caller may open this actuator.
#define NOMINMAX
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "cabinet_force.hpp"
#include "cabinet_recording.hpp"
#include "../sdl2/test_inject.hpp"
#include "wheel_device.hpp"
#include "../../../lib/toolkit/native/include/wheelffb.h"

namespace forcefeedback {
static bool owned_foreground() {
    const HWND window = GetForegroundWindow();
    DWORD process = 0;
    return window && GetWindowThreadProcessId(window, &process) &&
        process == GetCurrentProcessId() && !IsIconic(window);
}
static bool explicit_guid(const char* text, GUID& guid) {
    // Canonical instance GUID only. Never use product GUID or VID/PID fallback.
    if (!text) return false;
    const size_t length = std::strlen(text);
    if (length == 38 && text[0] == '{' && text[37] == '}') ++text;
    else if (length != 36) return false;
    unsigned char bytes[16]{};
    int nibble = 0;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') return false;
            continue;
        }
        const char c = text[i];
        const int v = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
            c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) return false;
        bytes[nibble / 2] = static_cast<unsigned char>((bytes[nibble / 2] << 4) | v);
        ++nibble;
    }
    guid.Data1 = (static_cast<DWORD>(bytes[0]) << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
    guid.Data2 = static_cast<WORD>((bytes[4] << 8) | bytes[5]);
    guid.Data3 = static_cast<WORD>((bytes[6] << 8) | bytes[7]);
    std::memcpy(guid.Data4, bytes + 8, 8);
    return true;
}
class ToolkitSink final : public CabinetSink {
    WheelFfbApi api{};
public:
    bool open(const unsigned char* guid, int hold_ms) override {
        if (!owned_foreground()) return false;
        if (!WheelFfb_LoadBeside(&api, nullptr, L"WheelFfb.dll")) return false;
        if (api.GetWheelFfbVersion() != 600) return false;
        api.SetStrictDeviceSelection(1);
        api.SetPreferredDeviceGuid(guid);
        api.SetHoldTimeoutMs(hold_ms);
        // Zero chooses this process's owned window without truncating an HWND.
        if (!api.InitDirectInput(0)) return false;
        return api.InstallExitGuards() != 0;
    }
    bool send(int force) override { return api.SetDeviceForcesXY && api.SetDeviceForcesXY(force, 0) != 0; }
    bool stop() override { return api.StopEffect && api.StopEffect() != 0; }
    bool transient_failure() override {
        if (!api.GetLastHResult) return false;
        const HRESULT hr = static_cast<HRESULT>(api.GetLastHResult());
        return hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED ||
            hr == DIERR_NOTEXCLUSIVEACQUIRED || hr == DIERR_INCOMPLETEEFFECT;
    }
    uint64_t now_ms() override { return GetTickCount64(); }
    void release() override {
        // Each operation remains independent of the previous return value.
        if (api.ZeroForces) api.ZeroForces();
        if (api.StopEffect) api.StopEffect();
        if (api.FreeDirectInput) api.FreeDirectInput();
        WheelFfb_Unload(&api);
    }
};
static ToolkitSink sink;
static CabinetForce controller(sink);
static std::string saved_guid;
// Keep the enumeration-only DLL reference until close. Unloading a library that
// still owns its read-only DirectInput instance would leak that instance. Never
// free shared native state while the controller might still own an actuator.
static WheelFfbApi catalog{};
std::vector<WheelDevice> enumerate_wheels() {
    std::vector<WheelDevice> result;
    if (!catalog.module && !WheelFfb_LoadBeside(&catalog, nullptr, L"WheelFfb.dll")) {
        WheelFfb_Unload(&catalog);
        return result;
    }
    if (catalog.GetWheelFfbVersion() != 600) return result;
    const int count = std::min(64, std::max(0, catalog.EnumerateDevices()));
    for (int i=0; i<count; ++i) {
        char label[256]{};
        GUID guid{};
        if (!catalog.GetDeviceName(i, label, sizeof(label)) || !catalog.GetDeviceGuid(i, &guid)) continue;
        const GUID zero{};
        if (!std::memcmp(&guid, &zero, sizeof(guid))) continue;
        label[255]=0;
        char identity[37]{};
        std::snprintf(identity, sizeof(identity), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
            guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        result.push_back({label, identity});
    }
    return result;
}
void configure_guid(const char* saved) { saved_guid = saved ? saved : ""; }
bool init(int maximum, int minimum, int duration) {
    // Diagnostic mode refuses before even loading the force library.
    const char* muted = std::getenv("DBCE_FFB_MUTE");
    if (cabinet_recording::requested() || (muted && std::strcmp(muted, "0") != 0)) return false;
    // A test injection requested at start rules force out for the process (sdl2/test_inject.hpp); otherwise this
    // claims the process for force, and no test injection arms after it.
    if (!test_inject::allow_force_output()) return false;
    GUID guid{};
    const char* override_guid = std::getenv("FF_TARGET_GUID");
    const char* selected_guid = override_guid && *override_guid ? override_guid : saved_guid.c_str();
    if (!explicit_guid(selected_guid, guid)) {
        std::fprintf(stderr, "Wheel FFB unavailable: choose a wheel in Controls, save settings and restart; no automatic selection.\n");
        return false;
    }
    const bool ready = controller.initialize(reinterpret_cast<const unsigned char*>(&guid), {maximum, minimum, duration});
    std::fprintf(stderr, "Wheel FFB cabinet-command@2: %s (explicit wheel, bounded hold; calibration pending).\n", ready ? "configured; waiting for foreground driving" : "refused");
    return ready;
}
void set_active(bool enabled) {
    if(cabinet_recording::requested()||test_inject::no_force())return;
    const bool was_ready = controller.supported();
    controller.set_active(enabled && owned_foreground());
    if (!was_ready && controller.supported())
        std::fprintf(stderr, "Wheel FFB opened for the selected wheel in foreground driving.\n");
    if (was_ready && !controller.supported())
        std::fprintf(stderr, "Wheel FFB stopped: neutral/stop was not acknowledged at gameplay or focus handback; no automatic reopen.\n");
}
int set(int command, int force) {
    if(cabinet_recording::requested()||test_inject::no_force())return -1;
    if (!owned_foreground()) controller.set_active(false);
    const bool was_ready = controller.supported();
    const bool recovering = controller.is_recovering();
    const bool accepted = controller.set(command, force);
    if (!recovering && controller.is_recovering())
        std::fprintf(stderr, "Wheel FFB transient refusal: neutral-gated recovery (2 foreground seconds, at most20 retries).\n");
    if (recovering && !controller.is_recovering() && controller.supported())
        std::fprintf(stderr, "Wheel FFB delivery recovered after an accepted neutral and command.\n");
    if (was_ready && !controller.supported())
        std::fprintf(stderr, "Wheel FFB stopped after invalid input or refused delivery; no automatic reopen.\n");
    return accepted ? 0 : -1;
}
void close() {
    controller.close();
    if (catalog.module) {
        if (catalog.FreeDirectInput) catalog.FreeDirectInput();
        WheelFfb_Unload(&catalog);
    }
}
bool is_supported() { return controller.supported(); }
} // namespace forcefeedback
