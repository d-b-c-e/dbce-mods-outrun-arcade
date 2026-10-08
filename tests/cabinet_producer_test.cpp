// Original engine arithmetic compiled verbatim by extract_cabinet_producer.py.
// No SDL, game assets, native wheel API, game process or physical output.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#define private public
#include "../src/main/engine/ooutputs.hpp"
#undef private
#include "../src/main/directx/cabinet_force.hpp"

enum { GS_INGAME = 12 };
struct { int game_state; } outrun;
struct { int16_t crash_counter, skid_counter; } ocrash;
struct { uint32_t car_increment; int16_t road_curve; } oinitengine;
struct OFerrari { enum { WHEELS_ON=0, WHEELS_OFF=3 }; uint8_t wheel_state; int16_t car_x_diff; } oferrari;
struct { int16_t steering_adjust; } oinputs;
namespace forcefeedback {
    static std::vector<std::pair<int,int>> requests;
    int set(int command, int step) { requests.emplace_back(command, step); return 0; }
}
#include "cabinet_producer.inc"

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

// All state read/written by the FFEEDBACK producer. Cabinet hardware limits and
// calibration state are deliberately outside this contract and mode.
using State = std::array<int,14>;
static State snapshot(const OOutputs& m) {
    return {m.hw_motor_control,m.motor_enabled,m.motor_x_change,m.motor_control,
        m.motor_movement,m.is_centered,m.motor_change_latch,m.speed,m.curve,
        m.counter,m.was_small_change,m.movement_adjust1,m.movement_adjust2,m.movement_adjust3};
}
static void restore(OOutputs& m, const State& s) {
    m.hw_motor_control=static_cast<uint8_t>(s[0]);m.motor_enabled=s[1]!=0;
    m.motor_x_change=static_cast<int16_t>(s[2]);m.motor_control=static_cast<int8_t>(s[3]);
    m.motor_movement=static_cast<int8_t>(s[4]);m.is_centered=s[5]!=0;
    m.motor_change_latch=static_cast<int16_t>(s[6]);m.speed=static_cast<int16_t>(s[7]);
    m.curve=static_cast<int16_t>(s[8]);m.counter=static_cast<int16_t>(s[9]);
    m.was_small_change=s[10]!=0;m.movement_adjust1=static_cast<int16_t>(s[11]);
    m.movement_adjust2=static_cast<int16_t>(s[12]);m.movement_adjust3=static_cast<int16_t>(s[13]);
}
struct Input {
    int game; int16_t crash,skid,curve,steering,motor,x_diff; uint32_t increment; uint8_t wheels;
};
struct Row { Input input; State before,after; int command,step,nominal; };
static Row advance(OOutputs& m, const Input& i) {
    outrun.game_state=i.game;ocrash.crash_counter=i.crash;ocrash.skid_counter=i.skid;
    oinitengine.road_curve=i.curve;oinitengine.car_increment=i.increment;
    oferrari.car_x_diff=i.x_diff;oferrari.wheel_state=i.wheels;oinputs.steering_adjust=i.steering;
    Row r{};r.input=i;r.before=snapshot(m);forcefeedback::requests.clear();
    m.do_motors(OOutputs::MODE_FFEEDBACK,i.motor);m.motor_output(m.hw_motor_control);
    r.after=snapshot(m);CHECK(forcefeedback::requests.size()==1);
    r.command=forcefeedback::requests[0].first;r.step=forcefeedback::requests[0].second;
    CHECK(forcefeedback::cabinet_force(r.command,r.step,{9000,8500,20},r.nominal));
    return r;
}
static void equal(const Row& a,const Row& b) {
    CHECK(a.before==b.before);CHECK(a.after==b.after);CHECK(a.command==b.command);
    CHECK(a.step==b.step);CHECK(a.nominal==b.nominal);
}
int main() {
    OOutputs m;m.init();
    // Independently readable fixed cases: neutral, stationary signs, original
    // seven-step mapping, then the exact eight-step crash waveform.
    Input i{12,0,0,0,0,128,0,0,0};
    auto r=advance(m,i);CHECK(r.command==0 && r.nominal==0);
    r=advance(m,i);CHECK(r.command==8 && r.nominal==0);
    i.motor=72;r=advance(m,i);CHECK(r.command==4 && r.step==3 && r.nominal==-8787);
    // At low speed with was_small_change=false the game retains its previous
    // command for this tick; preserve that behavior rather than invent symmetry.
    i.motor=184;r=advance(m,i);CHECK(r.command==4 && r.nominal==-8787);
    i.game=1;r=advance(m,i);CHECK(r.command==12 && r.step==3 && r.nominal==8787);
    m.init();i.game=12;i.motor=128;i.increment=200u<<16;i.crash=1;
    const int crash[]={8,8,2,2,8,8,14,14};
    for(int code:crash){r=advance(m,i);CHECK(r.command==code);}

    // Capture one original stateful trajectory, then replay from its first
    // checkpoint and from an arbitrary midstream checkpoint. No reset per row.
    std::vector<Row> tape;uint32_t random=0x14b0cafe;
    auto next=[&](){random=random*1664525u+1013904223u;return random;};
    m.init();
    for(int n=0;n<12000;++n){
        Input v{};v.game=n%191<170?12:1;
        v.crash=n%103<13?1:0;v.skid=n%83<9?-20:0;
        v.curve=static_cast<int16_t>(next()%150);
        v.motor=static_cast<int16_t>(72+next()%113);
        v.steering=static_cast<int16_t>((static_cast<int>(v.motor)-128)*256/112);
        v.increment=(next()%300u)<<16;v.wheels=static_cast<uint8_t>(next()%4);
        v.x_diff=static_cast<int16_t>(static_cast<int>(next()%3)-1);
        tape.push_back(advance(m,v));
    }
    for(size_t start:{size_t(0),size_t(7311)}){
        OOutputs replay;replay.init();restore(replay,tape[start].before);
        for(size_t n=start;n<tape.size();++n)equal(tape[n],advance(replay,tape[n].input));
    }
    // Show that checkpoint/history is required: a midstream reset is not
    // interchangeable with restoring the original producer state.
    OOutputs reset;reset.init();CHECK(snapshot(reset)!=tape[7311].before);
    OOutputs disabled;disabled.init();disabled.motor_enabled=false;
    r=advance(disabled,i);CHECK(r.command==0 && r.nominal==0);
    std::printf("PASS %u original cabinet producer checks; 12000 synthetic rows, two stateful replays; no native calls\n",checks);
}
