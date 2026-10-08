#pragma once
#include <algorithm>

// cabinet-command@2: the original seven-step nonzero map, explicit neutral,
// checked delivery. This is not a tyre model or an Art-normalized gain.
namespace forcefeedback {
struct CabinetSettings {
    int maximum, minimum, hold_ms;
    bool valid() const {
        return minimum >= 0 && maximum >= minimum && maximum <= 32767 && hold_ms > 0;
    }
};
inline bool cabinet_force(int command, int step, const CabinetSettings& s, int& result) {
    result = 0;
    if (!s.valid() || command < 0 || command > 15) return false;
    if (command == 0 || command == 8) return true;
    if (step < 0 || step > 7) return false;
    const int magnitude = std::min(10000, s.maximum - ((s.maximum - s.minimum) / 7) * step);
    result = command < 8 ? -magnitude : magnitude;
    return true;
}

struct CabinetSink {
    virtual ~CabinetSink() = default;
    virtual bool open(const unsigned char* guid, int hold_ms) = 0;
    virtual bool send(int force) = 0;
    virtual bool stop() = 0;
    virtual void release() = 0;
};

class CabinetForce {
    CabinetSink& sink;
    CabinetSettings settings{};
    bool opened = false, active = false, primed = false, fault = false;
    bool silence() {
        // Always attempt both. Void best-effort native helpers are not an ack.
        const bool zero = sink.send(0);
        const bool stopped = sink.stop();
        primed = false;
        return zero || stopped;
    }
    bool fail() {
        fault = true;
        if (opened) { silence(); sink.release(); }
        opened = active = primed = false;
        return false;
    }
public:
    explicit CabinetForce(CabinetSink& value) : sink(value) {}
    bool initialize(const unsigned char* guid, CabinetSettings value) {
        // One explicit startup attempt. Never reopen after a delivery fault.
        if (opened || fault || !guid || !value.valid()) return false;
        bool nonzero = false;
        for (int i = 0; i < 16; ++i) nonzero |= guid[i] != 0;
        if (!nonzero) return false;
        settings = value;
        opened = true; // even failed initialization requires partial cleanup
        if (!sink.open(guid, std::clamp(value.hold_ms, 100, 500))) return fail();
        if (!sink.send(0)) return fail(); // never StartEffect with stored force
        if (!silence()) return fail();
        return true;
    }
    bool supported() const { return opened && !fault; }
    bool set_active(bool enabled) {
        if (!supported()) return false;
        active = enabled;
        if (!enabled && primed && !silence()) return fail();
        return true;
    }
    bool set(int command, int step) {
        if (!supported()) return false;
        int value = 0;
        if (!cabinet_force(command, step, settings, value)) return fail();
        if (!active || value == 0) {
            if (primed && !silence()) return fail();
            return true;
        }
        if (!primed) {
            if (!sink.send(0)) return fail();
            primed = true;
        }
        if (!sink.send(value)) return fail();
        return true;
    }
    void close() {
        if (opened) {
            if (!silence()) fault = true;
            sink.release();
        }
        opened = active = primed = false;
    }
};
} // namespace forcefeedback
