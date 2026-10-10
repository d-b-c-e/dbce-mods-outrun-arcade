// Offline checks for src/main/sdl2/test_inject.cpp (dev-only test injection for the profile wheel, STD-033 section 6).
// No game, SDL, device or force: samples are applied to plain arrays in SDL units, as Input::inject_frame passes them.
#include "sdl2/test_inject.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static int g_checks, g_failed;
static void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) { ++g_failed; std::printf("FAIL %s\n", what.c_str()); }
}

#define WHEEL "{11111111-2222-3333-4444-555555555555}"
#define OTHER "{66666666-7777-8888-9999-aaaaaaaaaaaa}"

struct Stick
{
    int16_t axes[8] = {0, 0, -32768, 0, 0, -32768, 0, 0};   // pedals at rest at SDL -32768
    uint8_t buttons[40] = {};
    uint8_t hats[1] = {0};
    int apply(int axes_n = 8, int buttons_n = 40, int hats_n = 1)
    { return test_inject::apply(axes, axes_n, buttons, buttons_n, hats, hats_n); }
};

// The force latch is process-wide and permanent, so each order runs as its own process (two ctest entries).
static int latch(bool force_first)
{
    if (force_first) {
        check(test_inject::allow_force_output() && !test_inject::no_force(), "force output starts");
        check(!test_inject::latch_no_force() && !test_inject::no_force(), "a later test request is refused: force already started");
    } else {
        check(test_inject::latch_no_force() && test_inject::no_force(), "a test request latches no force");
        check(!test_inject::allow_force_output() && test_inject::latch_no_force(), "force output is refused for the process, the latch holds");
    }
    std::printf("test_inject latch (%s): %d checks, %d failed\n", force_first ? "force first" : "request first", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc > 1 && !std::strcmp(argv[1], "--request-first")) return latch(false);
    if (argc > 1 && !std::strcmp(argv[1], "--force-first")) return latch(true);
    std::string why;

    // The profile wheel's DirectInput identity: exactly one attached controller with its vendor/product, and it is ours.
    using D = test_inject::DiDevice;
    check(test_inject::identity_error({{WHEEL, 0x346E, 0x0006}}, 0x346E, 0x0006, WHEEL).empty(), "the one R12 is the profile's instance");
    check(test_inject::identity_error({{"{11111111-2222-3333-4444-555555555555}", 0x346E, 0x0006}, {OTHER, 0x346E, 0x0101}}, 0x346E, 0x0006,
                                      "{11111111-2222-3333-4444-555555555555}").empty(), "another product beside it is fine");
    check(test_inject::identity_error({{"{66666666-7777-8888-9999-AAAAAAAAAAAA}", 0x346E, 0x0006}}, 0x346E, 0x0006, OTHER).empty(),
          "instance text compares case-insensitively");
    check(test_inject::identity_error({}, 0x346E, 0x0006, WHEEL).find("no attached") != std::string::npos, "no wheel attached");
    check(test_inject::identity_error({{WHEEL, 0x346E, 0x0006}, {OTHER, 0x346E, 0x0006}}, 0x346E, 0x0006, WHEEL).find("two attached") != std::string::npos,
          "twins refused");
    check(test_inject::identity_error({{OTHER, 0x346E, 0x0006}}, 0x346E, 0x0006, WHEEL).find("another DirectInput instance") != std::string::npos,
          "another instance of the wheel refused");
    check(test_inject::identity_error({{WHEEL, 0x346E, 0x0006}}, 0x346E, 0x0006, "").find("no DirectInput instance") != std::string::npos,
          "no configured instance refused");
    (void)sizeof(D);
    check(!test_inject::armed() && !test_inject::test_command("inject raw axis 0 dev=" WHEEL " value=65535 ms=500", why) &&
          why.find("session") != std::string::npos, "not armed: commands refused");
    Stick s;
    check(s.apply() == 0 && s.axes[0] == 0, "not armed: the stick is untouched");
    check(!test_inject::test_arm("") && !test_inject::armed(), "no profile wheel instance: refused");
    check(!test_inject::test_arm("{1111}") && !test_inject::armed(), "a malformed instance: refused");
    check(test_inject::test_arm("{11111111-2222-3333-4444-555555555555}") && test_inject::armed(), "armed for the profile wheel");
    test_inject::test_clock(1000);

    // Raw samples on the profile wheel replace their objects in SDL units; another instance is refused.
    check(!test_inject::test_command("inject raw axis 0 dev=" OTHER " value=65535 ms=500", why) && why.find("profile wheel") != std::string::npos,
          "another instance refused");
    check(!test_inject::test_command("inject action throttle 1 ms=500", why), "actions refused (CannonBall keeps no profile request)");
    check(test_inject::test_command("inject raw axis 0 dev=" WHEEL " value=65535 ms=500", why), "steering sample: " + why);
    check(test_inject::test_command("inject raw axis 2 dev=" WHEEL " value=65535 ms=500 range=0..65535", why), "throttle sample");
    check(test_inject::test_command("inject raw button 31 dev=" WHEEL " value=1 ms=500", why), "button sample");
    check(test_inject::test_command("inject raw hat 0 18000 dev=" WHEEL " value=18000 ms=500", why), "hat sample");
    s = Stick();
    check(s.apply() == 4 && s.axes[0] == 32767 && s.axes[2] == 32767 && s.axes[5] == -32768 && s.buttons[31] == 1 && s.hats[0] == 4,
          "steer full right, throttle full, button 31 down, hat down; brake untouched");
    s = Stick();
    check(s.apply(2, 16, 0) == 1 && s.axes[0] == 32767 && s.buttons[31] == 0 && s.hats[0] == 0,
          "objects the stick does not have are never delivered");
    check(test_inject::test_command("inject raw axis 0 dev=" WHEEL " value=16384 ms=500", why), "half left");
    s = Stick();
    s.apply();
    check(s.axes[0] == -16384, "half left in SDL units");
    test_inject::test_clock(1600);
    s = Stick();
    check(s.apply() == 0 && s.axes[0] == 0 && s.axes[2] == -32768 && s.buttons[31] == 0, "samples end on time; the stick's values stand");

    // A closed stick drops its running samples.
    check(test_inject::test_command("inject raw axis 5 dev=" WHEEL " value=65535 ms=5000", why), "long brake sample");
    s = Stick();
    check(s.apply() == 1 && s.axes[5] == 32767, "brake full");
    test_inject::device_closed();
    s = Stick();
    check(s.apply() == 0 && s.axes[5] == -32768, "dropped when the stick closes");
    // A close ends injection for the process: a reopened or replacement stick takes no raw sample.
    check(!test_inject::armed() && !test_inject::test_command("inject raw axis 5 dev=" WHEEL " value=65535 ms=500", why) &&
          why.find("session") != std::string::npos, "after a close no command is accepted");
    check(test_inject::test_arm(WHEEL), "tests only: a fresh test arm for the remaining checks");
    test_inject::test_clock(1600);

    // The command file: this session's nonce first, then at most 32 commands in at most 4096 bytes.
    check(test_inject::test_file("nonce=test\r\ninject raw button 35 dev=" WHEEL " value=1 ms=500\r\n", why) == 1 && why.empty(), "file read");
    check(test_inject::test_file("nonce=other1234\ninject raw button 35 dev=" WHEEL " value=1 ms=500\n", why) == 0 && why.find("nonce") != std::string::npos,
          "another session's file unread");
    check(test_inject::test_file("inject raw button 35 dev=" WHEEL " value=1 ms=500\n", why) == 0 && why.find("nonce") != std::string::npos,
          "a file without a nonce unread");
    {
        std::string big = "nonce=test\n";
        for (int i = 0; i < 33; ++i) big += "inject raw button 35 dev=" WHEEL " value=1 ms=100\n";
        check(test_inject::test_file(big, why) == 32 && why.find("32 commands") != std::string::npos, "32 commands per file");
        check(test_inject::test_file(std::string("nonce=test\n") + std::string(4097, ' '), why) == 0 && why.find("4096") != std::string::npos,
              "over 4096 bytes unread");
    }

    // The session ends at its expiry.
    check(test_inject::test_command("inject raw axis 0 dev=" WHEEL " value=0 ms=5000", why), "sample before expiry");
    test_inject::test_expire(2000);
    test_inject::test_clock(2000);
    s = Stick();
    check(s.apply() == 0 && !test_inject::armed() && s.axes[0] == 0, "expiry drops samples and disarms");
    check(!test_inject::test_command("inject raw axis 0 dev=" WHEEL " value=0 ms=500", why), "no commands after expiry");

    // inject.on names the session.
    std::string out;
    check(test_inject::test_session("nonce=abcd1234\r\nexpires=1900\r\n", 1000, out) && out == "abcd1234", "session accepted");
    check(!test_inject::test_session("nonce=abcd1234\nexpires=1000\n", 1000, out) && out.find("expired") != std::string::npos, "expired");
    check(!test_inject::test_session("nonce=abcd1234\nexpires=4601\n", 1000, out) && out.find("hour") != std::string::npos, "beyond an hour");
    check(!test_inject::test_session("nonce=abc\nexpires=1900\n", 1000, out), "short nonce");
    check(!test_inject::test_session("nonce=abcd-1234\nexpires=1900\n", 1000, out), "non-alphanumeric nonce");
    check(!test_inject::test_session("nonce=abcd1234\n", 1000, out) && out.find("expires") != std::string::npos, "no expiry");

    std::printf("test_inject: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
