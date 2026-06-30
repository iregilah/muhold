// decoder.hpp — Byte-stream frame decoder (the ground-station receiver).
//
// This is the core of the project. It consumes one byte at a time, exactly as
// a UART receive interrupt would deliver them, and reconstructs frames using a
// small state machine. It never blocks and never allocates: all buffers are
// fixed-size members. Those two properties are what make the same code usable
// inside a real interrupt service routine on a microcontroller.
//
// Input  : a stream of bytes, fed one at a time via feed(). The stream may
//          contain arbitrary garbage before, between, or inside frames.
// Output : decoded Telemetry records from frames whose CRC checks out, plus a
//          DecoderStats block counting what happened. feed() returns true on
//          exactly the byte that completes a valid frame.

#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.hpp"

namespace satlink {

// The four states the receiver moves through. From any state, any anomaly
// (bad header, failed CRC) sends it straight back to HuntSync, and a fresh SYNC
// word re-aligns it from any state (see the continuous detection in feed()).
// It can never get wedged waiting forever inside a frame.
enum class RxState {
    HuntSync,     // searching the stream for the SYNC word
    ReadHeader,   // collecting the 4 header bytes
    ReadPayload,  // collecting payload_len payload bytes
    ReadCrc,      // collecting the 2 CRC bytes, then verifying
};

// Observability counters. The dashboard reads these; the test suite asserts on
// them. They only ever increase.
struct DecoderStats {
    uint64_t bytes_seen     = 0;  // every byte ever fed
    uint64_t frames_valid   = 0;  // frames whose CRC matched
    uint64_t frames_crc_err = 0;  // frames whose CRC failed
    uint64_t frames_dropped = 0;  // frames implied missing by sequence gaps
    uint64_t resyncs        = 0;  // headers rejected as malformed
};

class Decoder {
public:
    // Feed exactly one received byte. Returns true on the single byte that
    // completes a valid (CRC-checked) frame; the record is then readable via
    // last_packet(). Returns false otherwise. Never blocks, never allocates.
    bool feed(uint8_t byte);

    // Valid only immediately after feed() returned true.
    const Telemetry& last_packet() const { return last_; }
    uint16_t         last_seq() const { return last_seq_; }

    const DecoderStats& stats() const { return stats_; }
    RxState             state() const { return state_; }

private:
    // Return to hunting for SYNC and clear the per-frame work state.
    void reset_to_hunt();

    RxState  state_   = RxState::HuntSync;
    uint32_t sync_sr_ = 0;  // rolling 32-bit register holding the last 4 bytes

    // Holds header + payload (everything the CRC covers). Sized for the worst
    // case so a corrupted length field can never drive an over-read.
    uint8_t buf_[kHeaderSize + kMaxPayloadSize] = {};
    size_t  idx_ = 0;  // bytes collected so far into buf_

    uint8_t  payload_len_ = 0;  // length declared by the current header
    uint16_t seq_         = 0;  // sequence number from the current header
    uint16_t rx_crc_      = 0;  // CRC received in the current frame
    uint8_t  crc_bytes_   = 0;  // CRC bytes collected so far (0..2)

    Telemetry last_{};               // last successfully decoded record
    uint16_t  last_seq_      = 0;    // its sequence number
    bool      have_last_seq_ = false;  // false until the first valid frame

    DecoderStats stats_{};
};

} // namespace satlink
