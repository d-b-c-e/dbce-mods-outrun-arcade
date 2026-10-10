// Dev-only test injection through CannonBall's real Input handlers (src/main/sdl2/input.cpp + test_inject.cpp), on an
// SDL2 virtual joystick with the MOZA R12's vendor/product: no device, window, haptic or force (forcefeedback is a stub
// that is never reached). Checks the lifecycle Astra's review asked for: an injected held throttle, button and hat reach
// the game's input state; closing the stick releases them; reopening it starts neutral with no stale edge; a physical
// press after the reopen still reaches the game; and the session's expiry gives the physical state back.
// The controls are config.xml exactly as Wheelkit's CannonBall adapter writes it (tests/fixtures/wheelkit, the bytes of
// Wheelkit's cannonball-controls fixture), read by the game's own parser and Config::load's key paths (load_controls).
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdio>
#include <string>

#include "frontend/config.hpp"
#include "sdl2/input.hpp"
#include "sdl2/test_inject.hpp"

// What input.cpp links but this test never reaches: the full Config (config.cpp) and the force backend.
Config config;
Config::Config(void) {}
Config::~Config(void) {}
namespace forcefeedback
{
bool init(int, int, int) { return false; }
int set(int, int) { return -1; }
void close() {}
bool is_supported() { return false; }
}

static int g_checks, g_failed;
static void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failed; std::printf("FAIL %s\n", what); }
}

#define WHEEL "{00000000-0000-0000-0000-0000000000a1}"   // the fixture profile's DirectInput instance

// The game's own event dispatch (main.cpp process_events), for the joystick events this test produces.
static void pump()
{
    SDL_JoystickUpdate();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_JOYAXISMOTION: input.handle_joy_axis(&e.jaxis); break;
        case SDL_JOYBUTTONDOWN: input.handle_joy_down(&e.jbutton); break;
        case SDL_JOYBUTTONUP: input.handle_joy_up(&e.jbutton); break;
        case SDL_JOYHATMOTION: input.handle_joy_hat(&e.jhat); break;
        default: break;
        }
    }
}

static void command(const std::string& line)
{
    std::string why;
    check(test_inject::test_command(line, why), (line + ": " + why).c_str());
}

int main()
{
    SDL_SetMainReady();
    // Only the virtual wheel: every real joystick backend off, so no attached device is seen or touched (and a real
    // R12 on the test machine is not a second device with the same identity).
    for (const char* hint : {SDL_HINT_JOYSTICK_HIDAPI, SDL_HINT_JOYSTICK_RAWINPUT, SDL_HINT_DIRECTINPUT_ENABLED,
                             SDL_HINT_XINPUT_ENABLED, SDL_HINT_JOYSTICK_WGI})
        SDL_SetHint(hint, "0");
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) < 0) { std::printf("SDL_Init failed: %s\n", SDL_GetError()); return 2; }
    check(SDL_NumJoysticks() == 0, "no real joystick is visible to the test");
    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_WHEEL;
    desc.naxes = 8; desc.nbuttons = 64; desc.nhats = 1;
    desc.vendor_id = 0x346E; desc.product_id = 0x0006;
    desc.name = "MOZA R12 Base (virtual test wheel)";
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    if (index < 0) { std::printf("no virtual joystick: %s\n", SDL_GetError()); return 2; }
    SDL_Joystick* physical = SDL_JoystickOpen(index);   // the test's own handle: it plays the physical wheel
    SDL_JoystickSetVirtualAxis(physical, 2, -32768);     // accelerator at rest
    SDL_JoystickSetVirtualAxis(physical, 5, -31025);     // brake at its captured rest

    // Wheelkit's applied config.xml through the game's parser: the wheel by identity and instance, the axes with the
    // pedals' rest, start 35 and hat-down 130; the old buttons of the unrecorded wheel cleared; gears unchanged.
    xml_parser::ptree xml;
    if (!xml_parser::read_xml(DBCE_WHEELKIT_APPLIED, xml)) { std::printf("cannot read %s\n", DBCE_WHEELKIT_APPLIED); return 2; }
    config.load_controls(xml);
    const auto& c = config.controls;
    check(c.pad_device.set && c.pad_device.vendor == 0x346E && c.pad_device.product == 0x0006 && c.pad_device.name == "Fixture wheel" &&
          c.pad_device_instance == WHEEL, "pad_device: identity, name and instance");
    check(c.analog == 1 && c.axis[0] == 0 && c.axis[1] == 2 && c.axis[2] == 5 && c.axis[3] == -1 && !c.invert[1] && !c.invert[2],
          "analog on: wheel 0, accelerator 2, brake 5, not inverted");
    check(c.has_rest[1] && c.rest[1] == -32768 && c.has_rest[2] && c.rest[2] == -31025, "pedal rests");
    check(c.padconfig[4] == 35 && c.padconfig[9] == 130, "start 35, down hat 0 direction 2");
    check(c.padconfig[0] == -1 && c.padconfig[1] == -1 && c.padconfig[5] == -1 && c.padconfig[6] == -1 && c.padconfig[7] == -1 &&
          c.padconfig[8] == -1 && c.padconfig[2] == -1 && c.padconfig[3] == -1, "the old wheel's acc, brake, coin, menu, view and up cleared; no gear buttons");
    check(c.gear == 2 && c.pad_id == 2 && c.haptic == 0 && c.max_force == 8500, "gear mode and the untouched settings as the file has them");
    input.init(config.controls.pad_id, config.controls.keyconfig, config.controls.padconfig,
               config.controls.analog, config.controls.axis, config.controls.invert, config.controls.asettings);
    pump();
    input.open_joy();
    check(input.gamepad, "the profile wheel opens by identity");
    check(test_inject::test_arm(WHEEL) && test_inject::armed(), "armed");
    test_inject::test_clock(1000);
    input.inject_frame();
    check(input.a_accel == 0 && !input.is_pressed(Input::START) && !input.is_pressed(Input::DOWN), "physical rest before any sample");

    // Session 1: an injected held throttle, start button and hat-down reach the game through the applied
    // padconfig/axis settings; the session's expiry gives the physical state back.
    command("inject raw axis 2 dev=" WHEEL " value=65535 ms=5000");
    command("inject raw button 35 dev=" WHEEL " value=1 ms=5000");
    command("inject raw hat 0 18000 dev=" WHEEL " value=18000 ms=5000");
    input.inject_frame();
    check(input.a_accel == 255, "injected throttle reaches the game");
    check(input.is_pressed(Input::START), "injected start reaches the game");
    check(input.is_pressed(Input::DOWN), "injected hat-down reaches the game");
    input.frame_done();
    test_inject::test_expire(2000);
    test_inject::test_clock(2000);
    input.inject_frame();   // the session ends here (disarmed): samples dropped
    input.inject_frame();   // and the game takes the physical state back
    check(!test_inject::armed() && input.a_accel == 0 && !input.is_pressed(Input::START) && !input.is_pressed(Input::DOWN),
          "expiry gives the physical throttle, button and hat back");
    input.frame_done();

    // Session 2 (tests only: a fresh arm): held values, then the stick closes.
    check(test_inject::test_arm(WHEEL), "second session");
    test_inject::test_clock(3000);
    command("inject raw axis 2 dev=" WHEEL " value=65535 ms=5000");
    command("inject raw button 35 dev=" WHEEL " value=1 ms=5000");
    command("inject raw hat 0 18000 dev=" WHEEL " value=18000 ms=5000");
    input.inject_frame();
    check(input.a_accel == 255 && input.is_pressed(Input::START) && input.is_pressed(Input::DOWN), "held again");
    input.frame_done();
    // Closing the stick releases every injected held value in the game's state, and ends injection for the process.
    input.close_joy();
    check(input.a_accel == 0, "close releases the throttle");
    check(!input.is_pressed(Input::START) && !input.is_pressed(Input::DOWN), "close releases the button and the hat");
    check(!test_inject::armed(), "a close ends injection");
    input.frame_done();

    // Reopening (here the same virtual wheel stands in for a replacement with the same vendor/product) starts neutral,
    // with no edge, and takes no raw sample: the identity check named only the stick that was open.
    pump();
    input.open_joy();
    input.inject_frame();
    check(input.gamepad && input.a_accel == 0, "reopened stick: throttle at rest");
    check(!input.is_pressed(Input::START) && !input.has_pressed(Input::START) && !input.is_pressed(Input::DOWN), "reopened stick: no stale press or edge");
    {
        std::string why;
        check(!test_inject::test_command("inject raw axis 2 dev=" WHEEL " value=65535 ms=5000", why) && why.find("session") != std::string::npos,
              "replacement stick: raw commands refused");
        input.inject_frame();
        check(input.a_accel == 0, "replacement stick: nothing injected");
    }
    input.frame_done();

    // A physical press after the reopen still reaches the game (the game's own event path).
    SDL_JoystickSetVirtualButton(physical, 35, SDL_PRESSED);
    pump();
    input.inject_frame();
    check(input.is_pressed(Input::START), "a physical start after the reopen reaches the game");
    SDL_JoystickSetVirtualButton(physical, 35, SDL_RELEASED);
    pump();
    input.inject_frame();
    check(!input.is_pressed(Input::START), "and its release");
    input.close_joy();
    SDL_JoystickClose(physical);
    SDL_JoystickDetachVirtual(index);
    SDL_Quit();
    std::printf("input_inject: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
