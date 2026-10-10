#pragma once
// Test injection for the profile wheel (dbce-wheel-mod-toolkit STD-033 section 6, STD-034), development only (Windows).
//
// CannonBall reads the wheel through SDL events, which the DirectInput proxies' fence cannot reach, so running samples
// are applied to a snapshot of the opened stick's physical state once per frame (Input::inject_frame) and every object
// whose effective value changed goes through the game's own handlers (handle_axis, the padconfig buttons, the hat);
// when a sample ends, the physical value is delivered again. It arms once, at startup, only when all of these hold:
//   - the process is force-muted: DBCE_FFB_MUTE is set (not "0") at its start, so the force library never loads and
//     no SDL haptic opens (ffeedback_windows.inl, input.cpp), whatever config.xml or the menu say later;
//   - %LOCALAPPDATA%\dbce\outrun-arcade\inject.on names a session: "nonce=<8-64 letters/digits>" and
//     "expires=<unix seconds, UTC>" no more than an hour ahead;
//   - config.xml names the profile wheel: controls.pad_device vendor/product and its DirectInput instance (attribute
//     "instance", written by Wheelkit), which raw commands must name as dev=.
// Commands come from %LOCALAPPDATA%\dbce\outrun-arcade\inject.txt, read once each time it changes (at most every
// 100 ms), at most 4096 bytes: "nonce=<the session's>" first, then at most 32 raw commands in the toolkit grammar:
//   inject raw <axis|button|hat> <index> [angle] dev=<instance> value=<raw> ms=<50-15000> [range=<min>..<max>]
// Axis values are in the binding's range (default 0..65535); SDL's axis is that minus 32768 (the R12's numbering is
// qualified as equal in both, profile_device.hpp). A session takes at most 2000 commands and ends at its expiry;
// running samples are dropped then and whenever the stick closes.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace test_inject
{
// Startup, after config.xml is read and before the joystick or force opens. `instance` is controls.pad_device's
// instance attribute, `wanted` whether pad_device names a wheel. Returns the log lines (none when not requested).
std::vector<std::string> init(const std::string& instance, bool wanted);
bool armed();
// Reads new commands when the file changed (at most every 100 ms).
void poll();
// One frame of the stick in SDL units (axes -32768..32767, buttons 0/1, hats SDL bits), each as many as the stick
// has: running samples replace their objects in place. Returns how many objects carried one.
int apply(int16_t* axes, int axis_count, uint8_t* buttons, int button_count, uint8_t* hats, int hat_count);
// The stick closed (removed or shut down): its running samples never reach a reopened stick.
void device_closed();

// Tests only: arm without files or environment (nonce "test", no expiry), feed one command or a whole command file,
// end the session at a time, read an inject.on text, and run with an explicit clock.
bool test_arm(const std::string& instance);
bool test_command(const std::string& line, std::string& why);
int test_file(const std::string& text, std::string& why);
void test_expire(uint64_t expires_ms);
bool test_session(const std::string& text, uint64_t now_unix, std::string& nonce_or_why);
void test_clock(uint64_t now_ms);
}
