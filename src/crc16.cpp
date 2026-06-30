// crc16.cpp — bitwise implementation of CRC-16/CCITT-FALSE.

#include "crc16.hpp"

namespace satlink {

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    // CRC-16/CCITT-FALSE parameters.
    constexpr uint16_t kPoly = 0x1021;
    uint16_t crc = 0xFFFF;  // initial value

    for (size_t i = 0; i < len; ++i) {
        // Bring the next byte into the top of the 16-bit register.
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        // Process the 8 bits MSB-first. If the bit shifted out of the top is
        // set, the register is XOR'd with the polynomial; this is polynomial
        // long division in GF(2). The remainder left in `crc` is the CRC.
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ kPoly);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

} // namespace satlink
