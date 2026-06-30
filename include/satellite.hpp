// satellite.hpp — Simulated spacecraft telemetry source.
//
// Stands in for the real sensors. It produces smoothly varying, plausible
// housekeeping values as a function of mission time so the dashboard has
// something lifelike to display: a battery that charges and discharges over an
// orbit, board temperatures swinging through sunlight and shadow, a slowly
// rotating attitude, and a mode that follows the power state.

#pragma once

#include <cstdint>
#include <random>

#include "protocol.hpp"

namespace satlink {

class Satellite {
public:
    explicit Satellite(uint32_t seed = 1);

    // Produce the telemetry sample for mission time `t_ms` (ms since boot).
    Telemetry sample(uint32_t t_ms);

private:
    std::mt19937                           rng_;
    std::uniform_real_distribution<double> noise_{-1.0, 1.0};
};

} // namespace satlink
