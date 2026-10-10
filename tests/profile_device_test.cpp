// STD-033 rig-profile controls for CannonBall: the pure runtime pieces (src/main/frontend/profile_device.hpp).
#include "frontend/profile_device.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_checks, g_failed;
static void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failed; std::printf("FAIL %s\n", what); }
}

int main()
{
    using namespace profile_device;
    struct Dev { unsigned vendor, product; };
    std::vector<Dev> rig = {{0x1234, 0xBEAD}, {0x346E, 0x0024}, {0x346E, 0x0006}};   // vJoy-like, MOZA stalk, R12
    auto vendorOf = [&](int i) { return rig[i].vendor; };
    auto productOf = [&](int i) { return rig[i].product; };
    std::string why;

    // Identity resolution: the index follows the device, not its position.
    Want r12; r12.set = true; r12.vendor = 0x346E; r12.product = 0x0006; r12.name = "MOZA R12 Base";
    check(resolve(r12, 0, (int)rig.size(), vendorOf, productOf, why) == 2 && why.empty(), "R12 found at its current index");
    std::swap(rig[0], rig[2]);
    check(resolve(r12, 2, (int)rig.size(), vendorOf, productOf, why) == 0, "R12 followed after the order changed; pad_id ignored");
    rig.push_back({0x346E, 0x0006});
    check(resolve(r12, 0, (int)rig.size(), vendorOf, productOf, why) == -1 && why.find("two attached") != std::string::npos, "twins refused");
    rig.pop_back(); rig.erase(rig.begin());
    check(resolve(r12, 0, (int)rig.size(), vendorOf, productOf, why) == -1 && why.find("not attached") != std::string::npos,
          "missing device: joystick off, no pad_id fallback");
    Want other; other.set = true; other.vendor = 0x046D; other.product = 0xC24F;
    check(resolve(other, 0, (int)rig.size(), vendorOf, productOf, why) == -1 && why.find("not qualified") != std::string::npos, "unqualified device refused");
    Want none;
    check(resolve(none, 1, (int)rig.size(), vendorOf, productOf, why) == 1, "no identity: legacy pad_id");
    check(resolve(none, 5, (int)rig.size(), vendorOf, productOf, why) == -1, "legacy pad_id beyond the devices");

    // Pedals without a rest: today's linear (v + 0x8000) / 0x100, invert first.
    check(pedal(-32768, false, false, 0) == 0 && pedal(32767, false, false, 0) == 255 && pedal(0, false, false, 0) == 128, "legacy linear");
    check(pedal(-32768, true, false, 0) == 255 && pedal(32767, true, false, 0) == 0, "legacy invert (clamped at 255)");
    // With a rest: the owner's brake (DirectInput rest 1743 of 65535 is SDL -31025) reads 0 released, 255 full.
    check(pedal(-31025, false, true, -31025) == 0 && pedal(-32768, false, true, -31025) == 0, "brake released and below rest: 0");
    check(pedal(32767, false, true, -31025) == 255, "brake full: 255");
    check(pedal(872, false, true, -31025) == 128, "brake half travel: 128");
    // An inverted pedal resting at the top.
    check(pedal(32767, true, true, 32767) == 0 && pedal(-32768, true, true, 32767) == 255, "inverted pedal from rest to full");
    // Fail closed: rest at the full end, outside the range.
    check(pedal(32767, false, true, 32767) == 0 && pedal(-32768, true, true, -32768) == 0, "rest at the full end: 0");
    check(pedal(32767, false, true, 40000) == 0 && pedal(32767, false, true, -40000) == 0, "rest out of range: 0");
    check(pedal(100000, false, true, -31025) == 255 && pedal(-100000, false, true, -31025) == 0, "samples clamped to the SDL range");

    // Config values: four hex digits for an id; a plain decimal rest in the SDL range, else BadRest (always released).
    unsigned id = 0;
    check(parseId("346E", id) && id == 0x346E && parseId("0006", id) && id == 6 && parseId("beef", id) && id == 0xBEEF, "ids parse");
    check(!parseId("346", id) && !parseId("0x346E", id) && !parseId("34 6E", id) && !parseId("", id) && !parseId("346G", id), "malformed ids refused");
    check(parseRest("-31025") == -31025 && parseRest("0") == 0 && parseRest("32767") == 32767 && parseRest("-32768") == -32768, "rests parse");
    check(parseRest("32768") == BadRest && parseRest("-32769") == BadRest && parseRest("1e3") == BadRest && parseRest(" 5") == BadRest &&
          parseRest("-") == BadRest && parseRest("") == BadRest && parseRest("123456") == BadRest, "malformed rests are BadRest");
    check(pedal(32767, false, true, BadRest) == 0 && pedal(-32768, true, true, BadRest) == 0, "BadRest pedal reads released");

    // POV directions: 128 + hat * 4 + direction, SDL hat bits.
    check(isPov(128) && isPov(143) && !isPov(127) && !isPov(144), "POV range");
    check(povHat(128) == 0 && povHat(133) == 1, "POV hat index");
    check(povPressed(128, 0x01) && !povPressed(128, 0x02) && povPressed(129, 0x02) && povPressed(130, 0x04) && povPressed(131, 0x08), "straight directions");
    check(povPressed(128, 0x03) && povPressed(129, 0x03) && !povPressed(130, 0x03), "a diagonal holds both neighbours");
    check(!povPressed(128, 0x00) && !povPressed(12, 0x01), "centred hat; a button index is not a POV");

    std::printf("%s %d CannonBall profile-device checks\n", g_failed ? "FAIL" : "PASS", g_checks);
    return g_failed ? 1 : 0;
}
