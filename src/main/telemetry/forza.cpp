/***************************************************************************
    Forza Horizon "Data Out" telemetry (DBCE toolkit STD-016). See forza.hpp.
***************************************************************************/

#include "telemetry/forza.hpp"
#ifdef _WIN32
#include "directx/cabinet_recording.hpp"
#endif

#include <cstring>
#include <iostream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    static const socket_t BAD_SOCKET = INVALID_SOCKET;
    #define CLOSE_SOCKET closesocket
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    typedef int socket_t;
    static const socket_t BAD_SOCKET = -1;
    #define CLOSE_SOCKET ::close
#endif

namespace telemetry
{
    // Offsets: Dbce.Wheel.Telemetry.ForzaPacket (Horizon 324 layout).
    enum : int
    {
        OFF_IS_RACE_ON = 0, OFF_TIMESTAMP = 4, OFF_MAX_RPM = 8, OFF_IDLE_RPM = 12, OFF_RPM = 16,
        OFF_VELOCITY_X = 32, OFF_WHEEL_SPEED = 100, OFF_SURFACE_RUMBLE = 148,
        OFF_CAR_ORDINAL = 212, OFF_DRIVETRAIN = 224, OFF_CYLINDERS = 228,
        OFF_POSITION_X = 244, OFF_SPEED = 256, OFF_DISTANCE = 292, OFF_CURRENT_LAP = 304, OFF_RACE_TIME = 308,
        OFF_LAP_NUMBER = 312, OFF_RACE_POSITION = 314, OFF_ACCEL = 315, OFF_BRAKE = 316, OFF_GEAR = 319,
        OFF_STEER = 320, OFF_SENTINEL = 323
    };
    const uint8_t SENTINEL = 0x52;   // as the other DBCE mods: proves the packet came from a mod

    static void put_u32(uint8_t* b, int off, uint32_t v) { b[off] = (uint8_t) v; b[off + 1] = (uint8_t) (v >> 8); b[off + 2] = (uint8_t) (v >> 16); b[off + 3] = (uint8_t) (v >> 24); }
    static void put_f32(uint8_t* b, int off, float v) { uint32_t u; std::memcpy(&u, &v, 4); put_u32(b, off, u); }

    void build_packet(const Frame& f, uint8_t* b)
    {
        std::memset(b, 0, PACKET_SIZE);
        put_u32(b, OFF_IS_RACE_ON, f.race_on ? 1 : 0);
        put_u32(b, OFF_TIMESTAMP, f.timestamp_ms);
        put_f32(b, OFF_MAX_RPM, MAX_RPM);
        put_f32(b, OFF_IDLE_RPM, IDLE_RPM);
        float rpm = f.revs * RPM_PER_REV_UNIT;
        if (rpm > MAX_RPM) rpm = MAX_RPM;
        put_f32(b, OFF_RPM, rpm);

        const float speed = f.speed_kmh / 3.6f;
        put_f32(b, OFF_VELOCITY_X + 8, speed);                 // local forward (z)
        const float wheel_rad = speed / 0.32f;                 // 225/50 R16-ish rolling radius
        for (int w = 0; w < 4; w++) put_f32(b, OFF_WHEEL_SPEED + 4 * w, wheel_rad);
        // Off-road: rumble on the wheels that left the road (left = FL, RL; right = FR, RR).
        const bool left = f.wheels_off == 1 || f.wheels_off == 3, right = f.wheels_off == 2 || f.wheels_off == 3;
        put_f32(b, OFF_SURFACE_RUMBLE + 0, left ? 1.0f : 0.0f);
        put_f32(b, OFF_SURFACE_RUMBLE + 4, right ? 1.0f : 0.0f);
        put_f32(b, OFF_SURFACE_RUMBLE + 8, left ? 1.0f : 0.0f);
        put_f32(b, OFF_SURFACE_RUMBLE + 12, right ? 1.0f : 0.0f);

        put_u32(b, OFF_CAR_ORDINAL, 1986);                     // OutRun's year; any stable id
        put_u32(b, OFF_DRIVETRAIN, 1);                         // RWD
        put_u32(b, OFF_CYLINDERS, 12);                         // Testarossa flat-12

        put_f32(b, OFF_POSITION_X, f.car_x / 100.0f);
        put_f32(b, OFF_POSITION_X + 8, f.distance_m);
        put_f32(b, OFF_SPEED, speed);
        put_f32(b, OFF_DISTANCE, f.distance_m);
        put_f32(b, OFF_CURRENT_LAP, f.race_time_s);
        put_f32(b, OFF_RACE_TIME, f.race_time_s);
        b[OFF_LAP_NUMBER] = f.stage; b[OFF_LAP_NUMBER + 1] = 0;
        b[OFF_RACE_POSITION] = 0;
        b[OFF_ACCEL] = f.accel;
        b[OFF_BRAKE] = f.brake;
        b[OFF_GEAR] = f.gear;
        int steer = ((int) f.steering - 0x80) * 127 / 0x38;
        if (steer > 127) steer = 127; else if (steer < -127) steer = -127;
        b[OFF_STEER] = (uint8_t) (int8_t) steer;
        b[OFF_SENTINEL] = SENTINEL;
    }

    static socket_t sock = BAD_SOCKET;
    static sockaddr_in dest;
    static bool enabled_flag = false;
    static uint8_t packet[PACKET_SIZE];

    void init(bool enabled, const std::string& host, int port)
    {
        close();
        enabled_flag = enabled;
#ifdef _WIN32
        if(cabinet_recording::requested())enabled_flag=false;
#endif
        if (!enabled_flag) return;
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::cerr << "Telemetry: WSAStartup failed" << std::endl; enabled_flag = false; return; }
#endif
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == BAD_SOCKET) { std::cerr << "Telemetry: no UDP socket" << std::endl; enabled_flag = false; return; }
        std::memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons((uint16_t) port);
        if (inet_pton(AF_INET, host.c_str(), &dest.sin_addr) != 1)
        {
            std::cerr << "Telemetry: bad host " << host << std::endl;
            close(); return;
        }
        std::cout << "Telemetry: Forza Horizon Data Out to " << host << ":" << port << std::endl;
    }

    void tick(const Frame& f)
    {
        if (!enabled_flag || sock == BAD_SOCKET) return;
        build_packet(f, packet);
        sendto(sock, (const char*) packet, PACKET_SIZE, 0, (const sockaddr*) &dest, sizeof(dest));
    }

    void close()
    {
        if (sock != BAD_SOCKET)
        {
            // A final race-off packet, so dashboards drop to idle instead of freezing.
            std::memset(packet, 0, PACKET_SIZE); packet[OFF_SENTINEL] = SENTINEL;
            sendto(sock, (const char*) packet, PACKET_SIZE, 0, (const sockaddr*) &dest, sizeof(dest));
            CLOSE_SOCKET(sock);
            sock = BAD_SOCKET;
#ifdef _WIN32
            WSACleanup();
#endif
        }
        enabled_flag = false;
    }

    bool active() { return enabled_flag && sock != BAD_SOCKET; }
}
