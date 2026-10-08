/***************************************************************************
    Force Feedback (aka Haptic) Support — patched
    
    Linux: uses evdev (/dev/input/event*) with capability checks, stable
           device open/close, and a small bank of pre-uploaded effects.
    Windows: reviewed WheelFfb transport, explicit FF_TARGET_GUID selection,
             gameplay/focus gating, acknowledged neutral and bounded holding.
             Cabinet mapping remains distinct from tyre-force normalization.

    Copyright (c) 2025
***************************************************************************/

#include "ffeedback.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef __linux__
// --------------------------- Linux (evdev) ---------------------------
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <vector>
#include <string>


namespace forcefeedback {

static int fd = -1;
static bool g_supported = false;
static struct ff_effect effects[6]; // 0 unused, 1..5 = soft..strong (or vice versa)

static bool has_bit(const unsigned long* bits, int bit)
{
    return (bits[bit / (sizeof(unsigned long)*8)] >> (bit % (sizeof(unsigned long)*8))) & 1UL;
}

bool init(int max_force, int min_force, int duration_ms)
{
    if (fd >= 0) { g_supported = true; return true; }

    const char* devpath = "/dev/input/event";
    char device_file_name[64] = {0};

    for (int idx = 0; idx < 100; ++idx)
    {
        std::snprintf(device_file_name, sizeof(device_file_name), "%s%d", devpath, idx);
        int tmp = ::open(device_file_name, O_RDWR | O_CLOEXEC);
        if (tmp < 0) continue;

        // check this is a FF-capable device
        const size_t __BPL = sizeof(unsigned long) * 8;
        unsigned long ev_bits[(EV_MAX + __BPL - 1) / __BPL] = {};
        if (ioctl(tmp, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) == -1) { ::close(tmp); continue; }
        if (!has_bit(ev_bits, EV_FF)) { ::close(tmp); continue; }

        unsigned long ff_bits[(FF_MAX + __BPL - 1) / __BPL] = {};
        if (ioctl(tmp, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits) == -1) { ::close(tmp); continue; }
        bool have_rumble  = has_bit(ff_bits, FF_RUMBLE);
        bool have_period  = has_bit(ff_bits, FF_PERIODIC);
        if (!have_rumble && !have_period) { ::close(tmp); continue; }

        fd = tmp;
        break;
    }

    if (fd < 0) { g_supported = false; return false; }

    // Attempt to set overall gain (ignore failures)
    struct input_event ie{};
    ie.type = EV_FF;
    ie.code = FF_GAIN;
    ie.value = 0x7fff;
    (void)write(fd, &ie, sizeof(ie));

    // Upload a small bank of effects: 1..5
    for (int j = 1; j <= 5; ++j)
    {
        struct ff_effect e{};
        e.type = FF_RUMBLE;
        e.id = -1;
        e.u.rumble.strong_magnitude = (unsigned short)std::max(0, std::min(0x7fff, max_force - (j-1) * (max_force - min_force) / 4));
        e.u.rumble.weak_magnitude   = e.u.rumble.strong_magnitude / 2;
        e.replay.length = std::max(10, duration_ms);
        e.replay.delay  = 0;

        if (ioctl(fd, EVIOCSFF, &e) == -1)
        {
            // try periodic sine as fallback
            std::memset(&e, 0, sizeof(e));
            e.type = FF_PERIODIC;
            e.id = -1;
            e.u.periodic.waveform = FF_SINE;
            e.u.periodic.magnitude = (unsigned short)std::max(0, std::min(0x7fff, max_force - (j-1) * (max_force - min_force) / 4));
            e.u.periodic.period = 50;
            e.u.periodic.offset = 0;
            e.u.periodic.phase = 0;
            e.replay.length = std::max(10, duration_ms);
            e.replay.delay  = 0;

            if (ioctl(fd, EVIOCSFF, &e) == -1)
            {
                // give up on this device
                ::close(fd); fd = -1; g_supported = false;
                return false;
            }
        }
        effects[j] = e;
    }

    g_supported = true;
    return true;
}

int set(int command, int force) // command is unused; keep for ABI compatibility
{
    if (!g_supported || fd < 0) return -1;
    int idx = std::max(1, std::min(5, force));

    struct input_event play{};
    play.type = EV_FF;
    play.code = effects[idx].id;
    play.value = 1;
    if (write(fd, &play, sizeof(play)) == -1) return -1;

    return 0;
}

void close()
{
    if (fd >= 0)
    {
        // Try to stop all uploaded effects
        for (int j = 1; j <= 5; ++j)
        {
            if (effects[j].id >= 0)
            {
                struct input_event stop{};
                stop.type = EV_FF;
                stop.code = effects[j].id;
                stop.value = 0;
                (void)write(fd, &stop, sizeof(stop));
            }
        }
        ::close(fd);
        fd = -1;
    }
    g_supported = false;
}

bool is_supported()
{
    return g_supported;
}

} // namespace forcefeedback

#elif defined(_WIN32)
#include "ffeedback_windows.inl"

#else

// Stubs for other platforms
namespace forcefeedback {
bool init(int, int, int) { return false; }
int  set(int, int) { return -1; }
void close() {}
bool is_supported() { return false; }
}

#endif
