// channel.cpp — implementation of the bit-error-injecting channel.

#include "channel.hpp"

namespace satlink {

NoisyChannel::NoisyChannel(double bit_error_rate, uint32_t seed)
    : ber_(bit_error_rate), rng_(seed) {}

void NoisyChannel::transmit(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            ++total_bits_;
            // Draw a uniform [0,1) sample; if it lands below the BER, this bit
            // is unlucky and gets flipped with an XOR mask.
            if (dist_(rng_) < ber_) {
                data[i] ^= static_cast<uint8_t>(1u << bit);
                ++flipped_bits_;
            }
        }
    }
}

} // namespace satlink
