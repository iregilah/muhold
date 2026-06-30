// decoder.cpp — implementation of the byte-stream frame decoder.

#include "decoder.hpp"

#include "crc16.hpp"
#include "serialization.hpp"

namespace satlink {

void Decoder::reset_to_hunt() {
    state_      = RxState::HuntSync;
    idx_        = 0;
    crc_bytes_  = 0;
    rx_crc_     = 0;
    // We deliberately leave sync_sr_ untouched. It always holds the last four
    // bytes seen, so if a fresh SYNC word begins on the very next byte (frames
    // packed back to back), it is still detected. Clearing it would also be
    // safe, but keeping it is strictly more robust.
}

bool Decoder::feed(uint8_t byte) {
    ++stats_.bytes_seen;

    // --- Continuous SYNC detection (runs in EVERY state) --------------------
    // Roll the new byte into the low 8 bits of a 32-bit register, so sync_sr_
    // always holds the last four bytes received. Checking it on every byte --
    // not only while hunting -- means a SYNC word re-aligns the decoder from
    // ANY state. A truncated or corrupted frame can therefore never swallow the
    // start of the next frame: the instant a real SYNC appears, we restart on
    // it. This is what keeps the receiver from cascading one bad frame into
    // several lost ones, and it collapses all SYNC handling into one place.
    //
    // Trade-off: if a payload legitimately contained the 4-byte SYNC pattern it
    // would trigger a false restart. With a 32-bit marker the odds are about 1
    // in 4 billion per byte position; production systems remove even that with
    // fixed frame positions or byte stuffing. For this link it is negligible.
    sync_sr_ = (sync_sr_ << 8) | byte;
    if (sync_sr_ == kSyncWord) {
        if (state_ != RxState::HuntSync) {
            ++stats_.resyncs;  // abandoning a partial frame to realign
        }
        state_     = RxState::ReadHeader;
        idx_       = 0;
        crc_bytes_ = 0;
        return false;
    }

    switch (state_) {

    case RxState::HuntSync:
        // Waiting for SYNC, which is handled above. Nothing else to do here.
        return false;

    case RxState::ReadHeader: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize) {
            return false;  // still collecting the 4 header bytes
        }

        // Full header in hand: parse it.
        const uint8_t version = buf_[0];
        seq_         = get_u16(&buf_[1]);
        payload_len_ = buf_[3];

        // Defensive validation: never trust the input. A wrong version or an
        // unexpected length means this is not a frame we understand (garbage
        // happened to contain the SYNC pattern, or the header was corrupted),
        // so we discard it and resynchronize.
        if (version != kVersionType || payload_len_ != kPayloadSize) {
            ++stats_.resyncs;
            reset_to_hunt();
            return false;
        }

        state_ = RxState::ReadPayload;
        // idx_ stays at kHeaderSize; payload bytes continue filling buf_.
        return false;
    }

    case RxState::ReadPayload: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize + payload_len_) {
            return false;  // still collecting payload bytes
        }
        state_     = RxState::ReadCrc;
        crc_bytes_ = 0;
        rx_crc_    = 0;
        return false;
    }

    case RxState::ReadCrc: {
        // Assemble the two CRC bytes big-endian.
        rx_crc_ = static_cast<uint16_t>((rx_crc_ << 8) | byte);
        if (++crc_bytes_ < kCrcSize) {
            return false;  // need the second CRC byte
        }

        // Both CRC bytes received. Recompute the CRC over the buffered
        // header + payload and compare with what arrived.
        const uint16_t calc = crc16_ccitt(buf_, kHeaderSize + payload_len_);
        if (calc == rx_crc_) {
            // Valid frame. Decode the payload into engineering values.
            last_ = deserialize_telemetry(&buf_[kHeaderSize]);

            // Sequence-gap detection: if the new sequence number skipped past
            // the expected next one, that many frames were lost on the link.
            if (have_last_seq_) {
                const uint16_t expected = static_cast<uint16_t>(last_seq_ + 1);
                if (seq_ != expected) {
                    const uint16_t gap = static_cast<uint16_t>(seq_ - expected);
                    stats_.frames_dropped += gap;
                }
            }
            last_seq_      = seq_;
            have_last_seq_ = true;

            ++stats_.frames_valid;
            reset_to_hunt();
            return true;  // this byte completed a valid frame
        }

        // CRC mismatch: the frame was corrupted in transit. Drop it and resync.
        ++stats_.frames_crc_err;
        reset_to_hunt();
        return false;
    }
    }

    return false;  // unreachable; keeps the compiler happy
}

} // namespace satlink
