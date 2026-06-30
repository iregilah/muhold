// serialization.cpp — implementation of the big-endian (de)serializers.

#include "serialization.hpp"

namespace satlink {

// Big-endian = most significant byte at the lowest address. We shift the value
// right to bring each byte down to the low 8 bits, mask it, and store it. The
// & 0xFF is not strictly required after the cast but documents intent.

void put_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);  // high byte first
    p[1] = static_cast<uint8_t>(v & 0xFF);         // low byte
}

void put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}

// A signed 16-bit value has the same bit pattern as its unsigned counterpart in
// two's complement, so we reuse put_u16. The reader does the reverse cast.
void put_i16(uint8_t* p, int16_t v) {
    put_u16(p, static_cast<uint16_t>(v));
}

uint16_t get_u16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                 static_cast<uint16_t>(p[1]));
}

uint32_t get_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           (static_cast<uint32_t>(p[3]));
}

// Reassemble the 16 bits unsigned, then reinterpret as signed. Casting a
// uint16_t to int16_t reproduces the original two's-complement value on every
// real compiler (and is guaranteed from C++20 on).
int16_t get_i16(const uint8_t* p) {
    return static_cast<int16_t>(get_u16(p));
}

// The payload byte order below must match deserialize_telemetry exactly. Keep
// the two functions adjacent and edit them together.
//
//   [0..3]  timestamp_ms   (u32)
//   [4..5]  batt_mv        (u16)
//   [6..7]  temp_eps_c10   (i16)
//   [8..9]  temp_obc_c10   (i16)
//   [10..11] attitude_cdeg (i16)
//   [12]    mode           (u8)
size_t serialize_telemetry(const Telemetry& t, uint8_t* out) {
    put_u32(&out[0], t.timestamp_ms);
    put_u16(&out[4], t.batt_mv);
    put_i16(&out[6], t.temp_eps_c10);
    put_i16(&out[8], t.temp_obc_c10);
    put_i16(&out[10], t.attitude_cdeg);
    out[12] = static_cast<uint8_t>(t.mode);
    return kPayloadSize;
}

Telemetry deserialize_telemetry(const uint8_t* in) {
    Telemetry t;
    t.timestamp_ms  = get_u32(&in[0]);
    t.batt_mv       = get_u16(&in[4]);
    t.temp_eps_c10  = get_i16(&in[6]);
    t.temp_obc_c10  = get_i16(&in[8]);
    t.attitude_cdeg = get_i16(&in[10]);
    t.mode          = static_cast<Mode>(in[12]);
    return t;
}

} // namespace satlink
