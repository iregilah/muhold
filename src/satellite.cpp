// satellite.cpp — implementation of the simulated telemetry source.

#include "satellite.hpp"

#include <cmath>

namespace satlink {

namespace {
constexpr double kPi = 3.14159265358979323846;

// A sine that completes one cycle every `period_s` seconds, scaled to +-amp
// around `center`, evaluated at time `t_s`.
double wave(double t_s, double period_s, double center, double amp,
            double phase = 0.0) {
    return center + amp * std::sin(2.0 * kPi * t_s / period_s + phase);
}
}  // namespace

Satellite::Satellite(uint32_t seed) : rng_(seed) {}

Telemetry Satellite::sample(uint32_t t_ms) {
    const double t = t_ms / 1000.0;  // mission time in seconds

    Telemetry tm;
    tm.timestamp_ms = t_ms;

    // Battery bus voltage: a slow charge/discharge cycle (sped up to ~40 s for
    // a watchable demo), 3700 mV center, +-300 mV, with a little sensor noise.
    tm.batt_mv = static_cast<uint16_t>(
        wave(t, 40.0, 3700.0, 300.0) + 8.0 * noise_(rng_));

    // Board temperatures: a sunlight/shadow swing. The OBC lags the EPS board
    // slightly in phase and runs a touch cooler. Values are in 0.1 C steps, so
    // 235 means 23.5 C.
    tm.temp_eps_c10 = static_cast<int16_t>(
        wave(t, 30.0, 200.0, 150.0) + 2.0 * noise_(rng_));
    tm.temp_obc_c10 = static_cast<int16_t>(
        wave(t, 30.0, 180.0, 120.0, -0.6) + 2.0 * noise_(rng_));

    // Attitude angle: a slow tumble across +-180 deg. Stored in 0.01 deg steps,
    // so the full range is well within a signed 16-bit field (max +-327 deg).
    const double deg = wave(t, 25.0, 0.0, 180.0);
    tm.attitude_cdeg = static_cast<int16_t>(deg * 100.0);

    // Operating mode follows the power state: drop to Safe when the battery is
    // low, otherwise rotate through the active modes over time.
    if (tm.batt_mv < 3450) {
        tm.mode = Mode::Safe;
    } else {
        switch ((t_ms / 8000) % 3) {
            case 0:  tm.mode = Mode::Nominal; break;
            case 1:  tm.mode = Mode::Payload; break;
            default: tm.mode = Mode::Comms;   break;
        }
    }

    return tm;
}

} // namespace satlink
