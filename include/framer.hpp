// framer.hpp — Frame builder (the transmitter / satellite side).
//
// The Framer turns a Telemetry record into a complete on-the-wire frame:
// SYNC word + header + payload + CRC. It keeps a running sequence counter so
// the receiver can detect lost frames. On a real spacecraft this is the OBC
// code that hands a byte buffer to the radio's UART.

#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.hpp"

namespace satlink {

class Framer {
public:
    // Encode `t` into a complete frame written to `out`.
    //   out      : destination buffer
    //   out_cap  : capacity of `out` in bytes
    // Returns the number of bytes written (kFrameSize), or 0 if `out` is too
    // small. Writing into a caller-provided buffer (rather than returning a
    // heap vector) mirrors how an embedded TX path actually works.
    size_t encode(const Telemetry& t, uint8_t* out, size_t out_cap);

    // The sequence number that the next encode() call will stamp.
    uint16_t next_seq() const { return seq_; }

private:
    uint16_t seq_ = 0;  // increments per frame, wraps 65535 -> 0
};

} // namespace satlink
