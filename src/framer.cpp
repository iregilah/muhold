// framer.cpp — implementation of the frame builder.

#include "framer.hpp"

#include "crc16.hpp"
#include "serialization.hpp"

namespace satlink {

size_t Framer::encode(const Telemetry& t, uint8_t* out, size_t out_cap) {
    if (out_cap < kFrameSize) {
        return 0;  // refuse to write past the caller's buffer
    }

    size_t i = 0;

    // 1) SYNC word. Not covered by the CRC; it only marks where the frame
    //    starts so the receiver can lock on.
    put_u32(&out[i], kSyncWord);
    i += kSyncSize;

    // 2) Header. We remember where it starts because the CRC is computed from
    //    here (header + payload), not from the SYNC word.
    const size_t crc_region_start = i;
    out[i++] = kVersionType;
    put_u16(&out[i], seq_);
    i += 2;
    out[i++] = static_cast<uint8_t>(kPayloadSize);

    // 3) Payload: the serialized telemetry record.
    serialize_telemetry(t, &out[i]);
    i += kPayloadSize;

    // 4) CRC over header + payload (kCrcCoveredSize bytes), big-endian.
    const uint16_t crc = crc16_ccitt(&out[crc_region_start], kCrcCoveredSize);
    put_u16(&out[i], crc);
    i += kCrcSize;

    ++seq_;  // advance for the next frame; wraps naturally at 16 bits
    return i;
}

} // namespace satlink
