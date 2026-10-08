#pragma once
#include "cabinet_signal.hpp"
#include <cstdlib>

namespace cabinet_recording {
// A present startup capture key mutes delivery even if its request is invalid.
// It never changes a player's saved enabled setting or chooses a force device.
inline bool requested() noexcept {
    static const bool present=std::getenv("DBCE_CANNONBALL_CAPTURE")!=nullptr;
    return present;
}
void initialize(bool haptic,bool real_cabinet,forcefeedback::CabinetSettings settings) noexcept;
void service(bool driving,bool paused) noexcept;
void begin(uint32_t update,const cabinet_signal::Inputs&,const cabinet_signal::State&) noexcept;
void request(int command,int step) noexcept;
void delivery(int result) noexcept;
void end(const cabinet_signal::State&) noexcept;
void close() noexcept;
}
