// serialization.hpp — Big-endian byte (de)serialization for wire data.
//
// Why this exists at all: you must never just memcpy a struct onto the wire.
// The compiler is free to insert padding between fields, and the in-memory byte
// order depends on the CPU (x86 is little-endian). Two different boards would
// then disagree about what the bytes mean. The fix is to serialize field by
// field with an explicit, agreed byte order. Network and space protocols use
// big-endian (most significant byte first), so that is what we use here.

#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.hpp"

namespace satlink {

// --- Primitive big-endian put/get ------------------------------------------
// Each writes/reads exactly the named number of bytes at `p`. The caller is
// responsible for `p` having enough room; these never read or write past their
// own width.

void     put_u16(uint8_t* p, uint16_t v);  // 2 bytes, MSB first
void     put_u32(uint8_t* p, uint32_t v);  // 4 bytes, MSB first
void     put_i16(uint8_t* p, int16_t v);   // 2 bytes, MSB first (two's complement)

uint16_t get_u16(const uint8_t* p);        // read 2 bytes, MSB first
uint32_t get_u32(const uint8_t* p);        // read 4 bytes, MSB first
int16_t  get_i16(const uint8_t* p);        // read 2 bytes as signed

// --- Telemetry record (de)serialization ------------------------------------

// Write `t` as exactly kPayloadSize (13) bytes into `out`. Returns the number
// of bytes written (always kPayloadSize).
size_t serialize_telemetry(const Telemetry& t, uint8_t* out);

// Read exactly kPayloadSize (13) bytes from `in` back into a Telemetry record.
Telemetry deserialize_telemetry(const uint8_t* in);

} // namespace satlink
