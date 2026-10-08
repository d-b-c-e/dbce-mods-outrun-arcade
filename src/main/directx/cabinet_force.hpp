#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>

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
    virtual bool transient_failure() = 0;
    virtual uint64_t now_ms() = 0;
};

class CabinetForce {
    CabinetSink& sink;
    CabinetSettings settings{};
    unsigned char selected[16]{};
    bool configured = false, opened = false, active = false, primed = false, fault = false;
    bool recovering = false, budget_paused = false;
    uint64_t deadline = 0, retry_at = 0, paused_at = 0;
    int retries = 0;
    void pause_budget() {
        if (recovering && !budget_paused) { paused_at = sink.now_ms(); budget_paused = true; }
    }
    void resume_budget() {
        if (recovering && budget_paused) {
            const uint64_t paused = sink.now_ms() - paused_at;
            deadline += paused; budget_paused = false;
        }
    }
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
        recovering = budget_paused = false;
        return false;
    }
    bool refused(bool transient) {
        // The HRESULT must be captured BEFORE zero/stop overwrite it.
        if (!transient) return fail();
        if (!recovering) {
            recovering = true; budget_paused = false; retries = 0; deadline = sink.now_ms() + 2000;
        }
        silence(); // no nonzero until a later accepted prime
        retry_at = sink.now_ms() + 100;
        return false;
    }
public:
    explicit CabinetForce(CabinetSink& value) : sink(value) {}
    bool initialize(const unsigned char* guid, CabinetSettings value) {
        // Remember one explicit identity; open only when the game gate admits it.
        if (configured || fault || !guid || !value.valid()) return false;
        bool nonzero = false;
        for (int i = 0; i < 16; ++i) nonzero |= guid[i] != 0;
        if (!nonzero) return false;
        settings = value;
        std::memcpy(selected, guid, 16);
        configured = true;
        return true;
    }
    bool supported() const { return opened && !fault; }
    bool is_recovering() const { return recovering; }
    bool set_active(bool enabled) {
        if (!configured || fault) return false;
        const bool was_active = active;
        active = enabled;
        if (!enabled) {
            // Never pause a recovery budget while an old force is unacknowledged.
            // A failed gate-close release retires the handle instead.
            if (opened && was_active && (primed || recovering) && !silence()) return fail();
            if (recovering && was_active) pause_budget();
            return true;
        }
        if (!opened) {
            opened = true; // failed initialization still needs partial cleanup
            if (!sink.open(selected, std::clamp(settings.hold_ms, 100, 500))) return fail();
            if (!sink.send(0)) return refused(sink.transient_failure());
            primed = true; // no StartEffect; only this accepted zero starts it
        }
        return true;
    }
    bool set(int command, int step) {
        if (!supported()) return false;
        int value = 0;
        if (!cabinet_force(command, step, settings, value)) return fail();
        if (!active) {
            if (primed && !silence()) return fail();
            return true;
        }
        if (recovering) {
            // Successful neutral demand is not a failed nonzero retry. Keep the
            // episode and its attempt count, but exclude acknowledged idle time.
            if (value == 0 && primed && budget_paused) {
                if (sink.send(0)) return true;
                const bool transient = sink.transient_failure();
                resume_budget();
                return refused(transient);
            }
            resume_budget();
            const uint64_t now = sink.now_ms();
            if (now >= deadline || retries >= 20) return fail();
            if (now < retry_at) return false;
            if (value != 0) ++retries;
            retry_at = now + 100;
        }
        if (!primed) {
            if (!sink.send(0)) return refused(sink.transient_failure());
            primed = true;
        }
        if (!sink.send(value)) return refused(sink.transient_failure());
        // An active neutral is an acknowledged zero, not Stop/Start chatter.
        // Zero alone must not erase a persistent nonzero-delivery episode.
        if (value != 0) recovering = budget_paused = false;
        else pause_budget();
        return true;
    }
    void close() {
        if (opened) {
            if (!silence()) fault = true;
            sink.release();
        }
        configured = opened = active = primed = recovering = budget_paused = false;
    }
};
} // namespace forcefeedback
