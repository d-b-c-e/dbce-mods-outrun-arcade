// Included only on Windows. No gamepad-rumble caller may open this actuator.
#define NOMINMAX
#include <windows.h>
#include "cabinet_force.hpp"
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
bool init(int maximum, int minimum, int duration) {
    // Diagnostic mode refuses before even loading the force library.
    const char* muted = std::getenv("DBCE_FFB_MUTE");
    if (muted && std::strcmp(muted, "0") != 0) return false;
    GUID guid{};
    if (!explicit_guid(std::getenv("FF_TARGET_GUID"), guid)) {
        std::fprintf(stderr, "Wheel FFB unavailable: set FF_TARGET_GUID to the chosen wheel's instance GUID; no automatic selection.\n");
        return false;
    }
    const bool ready = controller.initialize(reinterpret_cast<const unsigned char*>(&guid), {maximum, minimum, duration});
    std::fprintf(stderr, "Wheel FFB cabinet-command@2: %s (explicit wheel, bounded hold; calibration pending).\n", ready ? "ready" : "refused");
    return ready;
}
void set_active(bool enabled) { controller.set_active(enabled && owned_foreground()); }
int set(int command, int force) {
    if (!owned_foreground()) controller.set_active(false);
    const bool was_ready = controller.supported();
    const bool accepted = controller.set(command, force);
    if (was_ready && !controller.supported())
        std::fprintf(stderr, "Wheel FFB stopped after invalid input or refused delivery; no automatic reopen.\n");
    return accepted ? 0 : -1;
}
void close() { controller.close(); }
bool is_supported() { return controller.supported(); }
} // namespace forcefeedback
