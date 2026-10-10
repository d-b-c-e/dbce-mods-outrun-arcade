#pragma once
// Rig-profile controls (dbce-wheel-mod-toolkit STD-033) for CannonBall: the pieces Wheelkit's canonical config.xml keys
// need at runtime. Pure (no SDL), so tests/profile_device_test.cpp checks them offline.
//   controls.pad_device.<xmlattr>.vendor/.product/.name  - the wheel by USB identity instead of the SDL index pad_id
//   controls.analog.axis.accel|brake.<xmlattr>.rest      - the pedal's released end, in SDL units (-32768..32767)
//   controls.padconfig.* >= 128                          - a POV hat direction: 128 + hat * 4 + direction (up, right,
//                                                          down, left), the encoding recomp-ui and OutRun 2006 use
#include <cstdint>
#include <string>

namespace profile_device
{
// Devices whose SDL2 joystick numbering was seen to equal their DirectInput numbering (axes 0/2/5 and buttons 31/32 in
// the owner's own CannonBall capture agree with Wheelkit's DirectInput capture). Others are refused, not guessed.
inline bool qualified(unsigned vendor, unsigned product) { return vendor == 0x346E && product == 0x0006; }

struct Want
{
    bool set = false;          // controls.pad_device present: the identity decides, pad_id is not used
    unsigned vendor = 0, product = 0;
    std::string name;
};

// The SDL joystick index to open, or -1. With no identity, the legacy pad_id (if it exists). With an identity, exactly
// one attached device must match and the device must be qualified; otherwise -1 and why (the joystick stays off).
template <typename VendorOf, typename ProductOf>
int resolve(const Want& want, int legacyPadId, int count, VendorOf vendorOf, ProductOf productOf, std::string& why)
{
    why.clear();
    if (!want.set) return legacyPadId >= 0 && legacyPadId < count ? legacyPadId : -1;
    if (!qualified(want.vendor, want.product)) { why = "the profile's device is not qualified for CannonBall"; return -1; }
    int found = -1, matches = 0;
    for (int i = 0; i < count; ++i)
        if (vendorOf(i) == want.vendor && productOf(i) == want.product) { if (!matches++) found = i; }
    if (matches == 0) { why = "the profile's device is not attached"; return -1; }
    if (matches > 1) { why = "two attached devices share the profile's vendor/product"; return -1; }
    return found;
}

// A pedal axis sample (SDL -32768..32767) -> 0..255. Without a rest: today's (v + 0x8000) / 0x100, with invert negating
// first. With a rest: the released end maps to 0 and the full end (32767, or -32768 when inverted) to 255. A rest at the
// full end or outside the range fails closed (always 0), so a bad key never produces throttle.
inline int pedal(int value, bool invert, bool hasRest, long rest)
{
    const long long v = value < -32768 ? -32768 : value > 32767 ? 32767 : value;
    if (!hasRest) {
        const long long w = invert ? -v : v;
        long long a = (w + 0x8000) / 0x100;
        return (int)(a < 0 ? 0 : a > 0xff ? 0xff : a);
    }
    if (rest < -32768 || rest > 32767) return 0;
    const long long full = invert ? -32768 : 32767;
    if ((long long)rest == full) return 0;
    const long long travel = full - rest, moved = v - rest;   // both signed the same way when the pedal is pressed
    if ((travel > 0 && moved <= 0) || (travel < 0 && moved >= 0)) return 0;
    long long a = (moved * 255 + travel / 2) / travel;
    return (int)(a < 0 ? 0 : a > 0xff ? 0xff : a);
}

// A USB id as the profile writer stores it: exactly four hex digits ("346E"). Anything else is malformed.
inline bool parseId(const std::string& s, unsigned& out)
{
    if (s.size() != 4) return false;
    unsigned v = 0;
    for (char c : s) {
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        v = v * 16 + (unsigned)d;
    }
    out = v;
    return true;
}

// A rest: a decimal SDL axis value (-32768..32767) and nothing else. A malformed rest becomes BadRest, which pedal()
// reads as always released.
constexpr long BadRest = -40000;
inline long parseRest(const std::string& s)
{
    size_t i = s.size() && s[0] == '-' ? 1 : 0;
    if (i == s.size() || s.size() - i > 5) return BadRest;
    long v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return BadRest;
        v = v * 10 + (s[i] - '0');
    }
    v = s[0] == '-' ? -v : v;
    return v < -32768 || v > 32767 ? BadRest : v;
}

constexpr int PovBase = 128, PovLast = PovBase + 4 * 4 - 1;
inline bool isPov(int binding) { return binding >= PovBase && binding <= PovLast; }
inline int povHat(int binding) { return (binding - PovBase) / 4; }
// SDL hat state (SDL_HAT_UP 1, RIGHT 2, DOWN 4, LEFT 8; diagonals combine) -> whether this direction is held.
inline bool povPressed(int binding, uint8_t hatState) { return isPov(binding) && (hatState & (1u << ((binding - PovBase) % 4))) != 0; }
}
