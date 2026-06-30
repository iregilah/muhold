// channel.hpp — A noisy communication channel (bit-error injector).
//
// This models the radio link between the spacecraft and the ground station.
// Every bit that passes through has an independent probability (the bit error
// rate, BER) of being flipped. This is exactly the fault-injection mechanism a
// test environment needs: it lets us prove the receiver detects corruption and
// recovers, without any real hardware or RF.

#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

namespace satlink {

class NoisyChannel {
public:
    // bit_error_rate : probability in [0,1] that any given bit is flipped.
    // seed           : seeds the RNG so runs are reproducible.
    NoisyChannel(double bit_error_rate, uint32_t seed);

    // Corrupt `data` in place: each of the 8*len bits is flipped independently
    // with probability `bit_error_rate`.
    void transmit(uint8_t* data, size_t len);

    void   set_bit_error_rate(double ber) { ber_ = ber; }
    double bit_error_rate() const { return ber_; }

    // Running totals, useful for the dashboard's link statistics.
    uint64_t total_bits() const { return total_bits_; }
    uint64_t flipped_bits() const { return flipped_bits_; }

private:
    double                                 ber_;
    std::mt19937                           rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
    uint64_t                               total_bits_   = 0;
    uint64_t                               flipped_bits_ = 0;
};

} // namespace satlink
