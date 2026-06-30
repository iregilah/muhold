// protocol.hpp — On-the-wire protocol definitions for the telemetry link.
//
// This file is the single source of truth for the frame format that the
// satellite (transmitter) and the ground station (receiver) must agree on.
// Everything else in the project is built around the constants and the
// Telemetry record defined here.
//
// Frame layout on the wire (23 bytes, all multi-byte fields big-endian):
//
//   Offset  Size  Field          Value / meaning              Covered by CRC?
//   ------  ----  -------------  ---------------------------  ---------------
//     0      4    SYNC           0x1A 0xCF 0xFC 0x1D          no
//     4      1    version_type   0x01                         yes
//     5      2    seq_count      increments per frame         yes
//     7      1    payload_len    13 (size of Telemetry)       yes
//     8     13    payload        serialized Telemetry         yes
//    21      2    crc16          CRC-16/CCITT-FALSE           (this is the CRC)
//
// The CRC is computed over the 17 bytes from offset 4..20 (header + payload).
// It deliberately does NOT include the SYNC word or the CRC field itself.

#pragma once

#include <cstddef>
#include <cstdint>

namespace satlink {

// CCSDS Attached Sync Marker (ASM). This exact 32-bit pattern marks the start
// of every frame on the wire. Using the real space-standard value makes this a
// recognizable, standards-based artifact rather than an arbitrary exercise.
constexpr uint32_t kSyncWord = 0x1ACFFC1D;

// Protocol version/type byte. The receiver rejects any frame whose first header
// byte is not this value, so an incompatible protocol revision can never be
// mistaken for valid data.
constexpr uint8_t kVersionType = 0x01;

// --- Field sizes in bytes ---------------------------------------------------
constexpr size_t kSyncSize    = 4;   // SYNC word
constexpr size_t kHeaderSize  = 4;   // version_type(1) + seq_count(2) + payload_len(1)
constexpr size_t kPayloadSize = 13;  // a serialized Telemetry record
constexpr size_t kCrcSize     = 2;   // CRC-16

// Total bytes of one complete frame on the wire.
constexpr size_t kFrameSize = kSyncSize + kHeaderSize + kPayloadSize + kCrcSize; // 23

// The CRC covers header + payload: every byte between the SYNC word and the
// CRC field. This is the most common place to get the protocol wrong, so it
// has its own named constant.
constexpr size_t kCrcCoveredSize = kHeaderSize + kPayloadSize; // 17

// Defensive upper bound used to size fixed receive buffers. The receiver never
// trusts a length field enough to allocate from it; it sizes for the worst case
// up front and rejects anything larger. (A length-driven heap allocation that
// trusts corrupted input is a classic buffer-overflow bug in flight software.)
constexpr size_t kMaxPayloadSize = 255;

// Spacecraft operating mode, transmitted as a single byte.
enum class Mode : uint8_t {
    Safe    = 0,  // minimal power draw, fault recovery
    Nominal = 1,  // normal housekeeping operations
    Payload = 2,  // science / payload instrument active
    Comms   = 3,  // high-power downlink in progress
};

// One telemetry sample: the housekeeping data the spacecraft reports.
//
// Note the use of fixed-width integers with explicit physical scaling. On a
// real bus you transmit compact scaled integers, not floats: a raw ADC count
// is converted to engineering units only when displayed. Temperatures are in
// 0.1 C steps and the attitude angle is in 0.01 deg steps; those scales were
// chosen so the full expected range still fits a signed 16-bit field.
struct Telemetry {
    uint32_t timestamp_ms  = 0;            // ms since boot, read from the RTC
    uint16_t batt_mv       = 0;            // battery bus voltage, millivolts
    int16_t  temp_eps_c10  = 0;            // EPS board temperature, 0.1 C steps
    int16_t  temp_obc_c10  = 0;            // OBC board temperature, 0.1 C steps
    int16_t  attitude_cdeg = 0;            // attitude angle, 0.01 deg steps (+-327 deg)
    Mode     mode          = Mode::Safe;   // operating mode
};

} // namespace satlink
