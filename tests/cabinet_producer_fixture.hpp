#pragma once
// Original engine arithmetic compiled verbatim by extract_cabinet_producer.py.
// No SDL, game assets, native wheel API, game process or physical output.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "../src/main/engine/ooutputs.hpp"
#include "../src/main/directx/cabinet_force.hpp"
#include "../src/main/directx/cabinet_signal.hpp"

enum { GS_INGAME = 12 };
struct { int game_state; } outrun;
struct { int16_t crash_counter, skid_counter; } ocrash;
struct { uint32_t car_increment; int16_t road_curve; } oinitengine;
struct OFerrari { enum { WHEELS_ON=0, WHEELS_OFF=3 }; uint8_t wheel_state; int16_t car_x_diff; } oferrari;
struct { int16_t steering_adjust; } oinputs;
namespace forcefeedback {
    static std::vector<std::pair<int,int>> requests;
    static int delivery_result=-1;
    int set(int command, int step) { requests.emplace_back(command, step); return delivery_result; }
}
#ifndef CABINET_RECORDING_REAL
namespace cabinet_recording { void request(int,int) noexcept {} void delivery(int) noexcept {} }
#endif
#include "cabinet_producer.inc"

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

// All state read/written by the FFEEDBACK producer. Cabinet hardware limits and
// calibration state are deliberately outside this contract and mode.
using State = std::array<int,14>;
static State snapshot(const OOutputs& m) {
    return m.cabinet_snapshot();
}
struct CabinetReplayAccess {
static void restore(OOutputs& m, const State& s) {
    m.hw_motor_control=static_cast<uint8_t>(s[0]);m.motor_enabled=s[1]!=0;
    m.motor_x_change=static_cast<int16_t>(s[2]);m.motor_control=static_cast<int8_t>(s[3]);
    m.motor_movement=static_cast<int8_t>(s[4]);m.is_centered=s[5]!=0;
    m.motor_change_latch=static_cast<int16_t>(s[6]);m.speed=static_cast<int16_t>(s[7]);
    m.curve=static_cast<int16_t>(s[8]);m.counter=static_cast<int16_t>(s[9]);
    m.was_small_change=s[10]!=0;m.movement_adjust1=static_cast<int16_t>(s[11]);
    m.movement_adjust2=static_cast<int16_t>(s[12]);m.movement_adjust3=static_cast<int16_t>(s[13]);
}
static void poison_unrecorded(OOutputs& m) {
    m.limit_left=-199;m.limit_right=231;m.motor_centre_pos=-987;m.motor_state=634;
    m.vibrate_counter=3210;m.hw_motor_control_old=199;m.dig_out=231;m.dig_out_old=133;
    m.col1=999;m.col2=654;for(int n=0;n<3;++n){m.chute1.counter[n]=uint8_t(71+n);m.chute2.counter[n]=uint8_t(91+n);}
    m.chute1.output_bit=12;m.chute2.output_bit=11;m.mode=333;
}
static void step(OOutputs& m,int16_t motor) { m.do_motors(OOutputs::MODE_FFEEDBACK,motor);m.motor_output(m.hw_motor_control); }
static void enabled(OOutputs& m,bool value) { m.motor_enabled=value; }
};
static void restore(OOutputs& m,const State& s) { CabinetReplayAccess::restore(m,s); }
static void poison_unrecorded(OOutputs& m) { CabinetReplayAccess::poison_unrecorded(m); }
struct Input {
    int game; int16_t crash,skid,curve,steering,motor,x_diff; uint32_t increment; uint8_t wheels;
};
struct Row { Input input; State before,after; int command,step,nominal; };
static Row advance(OOutputs& m, const Input& i,forcefeedback::CabinetSettings settings={9000,8500,20}) {
    outrun.game_state=i.game;ocrash.crash_counter=i.crash;ocrash.skid_counter=i.skid;
    oinitengine.road_curve=i.curve;oinitengine.car_increment=i.increment;
    oferrari.car_x_diff=i.x_diff;oferrari.wheel_state=i.wheels;oinputs.steering_adjust=i.steering;
    Row r{};r.input=i;r.before=snapshot(m);forcefeedback::requests.clear();
    CabinetReplayAccess::step(m,i.motor);
    r.after=snapshot(m);CHECK(forcefeedback::requests.size()==1);
    r.command=forcefeedback::requests[0].first;r.step=forcefeedback::requests[0].second;
    CHECK(forcefeedback::cabinet_force(r.command,r.step,settings,r.nominal));
    return r;
}
static void equal(const Row& a,const Row& b) {
    CHECK(a.before==b.before);CHECK(a.after==b.after);CHECK(a.command==b.command);
    CHECK(a.step==b.step);CHECK(a.nominal==b.nominal);
}
static cabinet_signal::Frame frame(const Row& r,uint64_t us,uint32_t update) {
    const auto& i=r.input;
    return {us,update,{i.game,i.crash,i.skid,i.increment,i.curve,i.wheels,i.x_diff,i.steering,i.motor},
        r.before,r.after,r.command,r.step,r.nominal,-1};
}
