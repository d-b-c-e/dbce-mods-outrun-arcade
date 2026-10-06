/***************************************************************************
    Forza Horizon "Data Out" telemetry (DBCE toolkit STD-016).

    Sends the 324-byte Forza Horizon 4/5 packet over UDP, built from the
    engine's own state each game tick, so SimHub, dashboards and bass shakers
    work with OutRun the same way they do with the other DBCE racing mods.
    Layout: dbce-wheel-mod-toolkit Dbce.Wheel.Telemetry.ForzaPacket.
***************************************************************************/

#pragma once

#include <cstdint>
#include <string>

namespace telemetry
{
    const int PACKET_SIZE = 324;

    // Engine values the packet is built from (pure, so it can be tested without the game).
    struct Frame
    {
        bool     race_on;       // in game, including the start countdown
        uint32_t timestamp_ms;
        uint16_t speed_kmh;     // car_increment >> 16
        uint16_t revs;          // oferrari.revs >> 16 (HUD rev counter units)
        uint8_t  gear;          // 1 low, 2 high
        uint8_t  accel;         // 0..255
        uint8_t  brake;         // 0..255
        int16_t  steering;      // raw 0x48..0xB8, centre 0x80
        uint8_t  wheels_off;    // 0 on road, 1 left off, 2 right off, 3 both off
        int16_t  car_x;         // road-relative lateral position
        float    distance_m;
        float    race_time_s;
        uint8_t  stage;         // 0-based stage number (0..4)
    };

    // Engine units to Forza units (documented in docs/TELEMETRY.md).
    const float RPM_PER_REV_UNIT = 25.0f;   // HUD bar = 16 rev units; 20 bars = 8000 rpm
    const float MAX_RPM = 8000.0f;
    const float IDLE_RPM = 1000.0f;

    void build_packet(const Frame& f, uint8_t* out);   // out: PACKET_SIZE bytes

    void init(bool enabled, const std::string& host, int port);
    void tick(const Frame& f);
    void close();
    bool active();
}