// crc16.hpp — CRC-16/CCITT-FALSE error-detection code.
//
// A CRC (cyclic redundancy check) is a short checksum appended to a message so
// the receiver can detect transmission errors. The sender computes it over the
// data and sends it along; the receiver recomputes it over the received data
// and compares. If even one bit was corrupted in transit, the recomputed value
// almost certainly differs and the frame is rejected.
//
// We use the variant commonly labelled "CRC-16/CCITT-FALSE":
//   polynomial 0x1021, initial value 0xFFFF, no input/output bit reflection,
//   no final XOR. Its well-known check value is 0x29B1 for the ASCII string
//   "123456789", which the test suite asserts so you can verify the
//   implementation against any online CRC calculator.

#pragma once

#include <cstddef>
#include <cstdint>

namespace satlink {

// Compute the CRC-16/CCITT-FALSE of `len` bytes starting at `data`.
uint16_t crc16_ccitt(const uint8_t* data, size_t len);

} // namespace satlink
