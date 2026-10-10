// Dev-only test injection through CannonBall's real Input handlers (src/main/sdl2/input.cpp + test_inject.cpp), on an
// SDL2 virtual joystick with the MOZA R12's vendor/product: no device, window, haptic or force (forcefeedback is a stub
// that is never reached). Checks the lifecycle Astra's review asked for: an injected held throttle, button and hat reach
// the game's input state; closing the stick releases them; reopening it starts neutral with no stale edge; a physical
// press after the reopen still reaches the game; and the session's expiry gives the physical state back.
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

#define WHEEL "{11111111-2222-3333-4444-555555555555}"

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

    // config.xml as Wheelkit's CannonBall adapter writes it for the profile (pad_device, rests, start 35, hat-down 130).
    config.controls.pad_device.set = true;
    config.controls.pad_device.vendor = 0x346E;
    config.controls.pad_device.product = 0x0006;
    config.controls.has_rest[1] = true; config.controls.rest[1] = -32768;
    config.controls.has_rest[2] = true; config.controls.rest[2] = -31025;
    int keys[12] = {0};
    int pads[15];
    for (int& p : pads) p = -1;
    pads[4] = 35;    // start
    pads[9] = 130;   // down: hat 0, direction 2
    int axes[4] = {0, 2, 5, -1};
    bool invert[3] = {false, false, false};
    int analog[2] = {0, 0};
    input.init(0, keys, pads, 1, axes, invert, analog);
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
