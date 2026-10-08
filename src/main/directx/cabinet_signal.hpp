#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "cabinet_force.hpp"

// cannonball.cabinet-signal@1: original Windows cabinet producer observations.
// No game, platform, file, device or clock calls. This is NOT delivered torque.
namespace cabinet_signal {
using State = std::array<int32_t,14>;
using Inputs = std::array<int64_t,9>;
enum InputIndex { Game, Crash, Skid, Increment, Curve, Wheels, XDiff, Steering, Motor };
inline constexpr const char* input_names[] = {
    "gameState","crashCounter","skidCounter","carIncrement","roadCurve",
    "wheelState","carXDiff","steeringAdjust","motorInput"};
inline constexpr const char* state_names[] = {
    "command","enabled","positionChange","control","movement","centred",
    "movementLatch","speedIndex","curveIndex","counter","smallChange",
    "steeringHistory1","steeringHistory2","steeringHistory3"};
struct Frame {
    uint64_t elapsed_us{};
    uint32_t update{};
    Inputs inputs{};
    State before{},after{};
    int command{},step{},nominal{};
    int delivery_result=-2; // unset is distinct from the mute guard's -1
};
inline bool in(int64_t n,int64_t lo,int64_t hi) { return n>=lo && n<=hi; }
inline bool valid_inputs(const Inputs& i) {
    return i[Game]==12 && in(i[Crash],INT16_MIN,INT16_MAX) && in(i[Skid],INT16_MIN,INT16_MAX) &&
        in(i[Increment],0,UINT32_MAX) && in(i[Curve],INT16_MIN,INT16_MAX) && in(i[Wheels],0,3) &&
        in(i[XDiff],INT16_MIN,INT16_MAX) && in(i[Steering],-127,127) && in(i[Motor],0x48,0xb8);
}
inline bool valid_state(const State& s) {
    if(!in(s[0],0,15) || !in(s[1],0,1) || !in(s[3],INT8_MIN,INT8_MAX) ||
       s[4]!=0 || !in(s[5],0,1) || !in(s[10],0,1)) return false;
    for(int n:{2,6,7,8,9,11,12,13}) if(!in(s[n],INT16_MIN,INT16_MAX)) return false;
    return true;
}
inline bool valid_frame(const Frame& f,const forcefeedback::CabinetSettings& settings) {
    if(!valid_inputs(f.inputs) || !valid_state(f.before) || !valid_state(f.after) ||
       f.after[0]!=f.command || f.delivery_result!=-1) return false;
    const int step=f.command==0 || f.command==8 ? 0 : f.command<8 ? f.command-1 : 15-f.command;
    int nominal=0;
    return f.step==step && forcefeedback::cabinet_force(f.command,f.step,settings,nominal) && nominal==f.nominal;
}
enum class End { None, Duration, Interrupted, Overflow, Invalid, Discontinuity };
class Buffer {
    std::vector<Frame> frames_;
    forcefeedback::CabinetSettings settings_{};
    uint64_t duration_us_{};
    size_t capacity_{};
    End end_=End::None;
    bool armed_=false;
public:
    // Allocate before entering the producer; append never grows the allocation.
    bool arm(forcefeedback::CabinetSettings settings,unsigned seconds,size_t capacity=8192) {
        if(armed_ || !frames_.empty() || !settings.valid() || seconds<1 || seconds>120 || capacity<2 || capacity>8192) return false;
        frames_.reserve(capacity);settings_=settings;duration_us_=uint64_t(seconds)*1000000;
        capacity_=capacity;armed_=true;return true;
    }
    bool active() const { return armed_ && end_==End::None; }
    bool complete() const { return end_==End::Duration; }
    End reason() const { return end_; }
    const std::vector<Frame>& frames() const { return frames_; }
    const forcefeedback::CabinetSettings& settings() const { return settings_; }
    void stop(End why=End::Interrupted) noexcept {
        if(active()) end_=(why==End::None || why==End::Duration)?End::Interrupted:why;
    }
    bool append(const Frame& f) noexcept {
        if(!active()) return false;
        if(!valid_frame(f,settings_)){end_=End::Invalid;return false;}
        if(frames_.empty()) {
            if(f.elapsed_us!=0){end_=End::Discontinuity;return false;}
        } else {
            const auto& p=frames_.back();
            if(f.elapsed_us<=p.elapsed_us || f.elapsed_us-p.elapsed_us>1000000 ||
               p.update==UINT32_MAX || f.update!=p.update+1 || f.before!=p.after) {
                end_=End::Discontinuity;return false;
            }
        }
        if(frames_.size()==capacity_){end_=End::Overflow;return false;}
        frames_.push_back(f);
        if(f.elapsed_us>=duration_us_) end_=End::Duration;
        return true;
    }
};
} // namespace cabinet_signal
