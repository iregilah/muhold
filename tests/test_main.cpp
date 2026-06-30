// test_main.cpp — the test ladder.
//
// Each test proves one concrete property, building from the bottom up: CRC,
// then serialization, then framing, then the decoder state machine, and finally
// a fuzz/torture test. If every rung is green, the receiver behaves correctly
// even on a garbage-filled, bit-flipping link -- which is exactly the kind of
// robustness the job description asks for under "software testing methods".

#include <unistd.h>  // isatty

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "channel.hpp"
#include "crc16.hpp"
#include "decoder.hpp"
#include "framer.hpp"
#include "protocol.hpp"
#include "serialization.hpp"

using namespace satlink;

// --- Minimal assertion framework -------------------------------------------
namespace {

int g_checks = 0;
int g_fails  = 0;
bool g_color = false;

const char* red()   { return g_color ? "\x1b[31m" : ""; }
const char* green() { return g_color ? "\x1b[32m" : ""; }
const char* cyan()  { return g_color ? "\x1b[36m" : ""; }
const char* bold()  { return g_color ? "\x1b[1m"  : ""; }
const char* reset() { return g_color ? "\x1b[0m"  : ""; }

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_fails;                                                     \
            std::printf("    %s[FAIL]%s %s:%d  %s\n", red(), reset(),      \
                        __FILE__, __LINE__, #cond);                        \
        }                                                                  \
    } while (0)

void section(const char* name) {
    std::printf("%s%s>> %s%s\n", bold(), cyan(), name, reset());
}

// --- Helpers ----------------------------------------------------------------

Telemetry make_sample() {
    Telemetry t;
    t.timestamp_ms  = 123456;
    t.batt_mv       = 3700;
    t.temp_eps_c10  = 235;    // +23.5 C
    t.temp_obc_c10  = -120;   // -12.0 C  (exercises a signed/negative field)
    t.attitude_cdeg = -4500;  // -45.00 deg
    t.mode          = Mode::Nominal;
    return t;
}

bool same(const Telemetry& a, const Telemetry& b) {
    return a.timestamp_ms == b.timestamp_ms && a.batt_mv == b.batt_mv &&
           a.temp_eps_c10 == b.temp_eps_c10 && a.temp_obc_c10 == b.temp_obc_c10 &&
           a.attitude_cdeg == b.attitude_cdeg && a.mode == b.mode;
}

std::vector<uint8_t> build_frame(Framer& f, const Telemetry& t) {
    uint8_t buf[kFrameSize];
    const size_t n = f.encode(t, buf, sizeof(buf));
    return std::vector<uint8_t>(buf, buf + n);
}

// Feed a whole byte sequence; return true if any byte completed a valid frame.
bool feed_all(Decoder& d, const std::vector<uint8_t>& bytes) {
    bool got = false;
    for (uint8_t b : bytes) {
        if (d.feed(b)) got = true;
    }
    return got;
}

// --- Tests ------------------------------------------------------------------

void test_crc() {
    section("CRC-16/CCITT-FALSE");
    // The canonical check value: CRC of the ASCII string "123456789" is 0x29B1.
    // You can confirm this against any online CRC calculator.
    const char* s = "123456789";
    CHECK(crc16_ccitt(reinterpret_cast<const uint8_t*>(s), 9) == 0x29B1);

    // Empty input leaves the CRC at its initial value.
    CHECK(crc16_ccitt(reinterpret_cast<const uint8_t*>(s), 0) == 0xFFFF);
}

void test_serialization() {
    section("Serialization round-trip and byte order");
    const Telemetry tx = make_sample();
    uint8_t buf[kPayloadSize];
    const size_t n = serialize_telemetry(tx, buf);
    CHECK(n == kPayloadSize);

    // Round-trip must reproduce every field, including the negative ones.
    const Telemetry rx = deserialize_telemetry(buf);
    CHECK(same(tx, rx));

    // Explicit big-endian check: batt_mv 3700 = 0x0E74 -> 0x0E then 0x74.
    CHECK(buf[4] == 0x0E && buf[5] == 0x74);

    // A primitive put/get round-trip for a negative value.
    uint8_t p[2];
    put_i16(p, -1);
    CHECK(p[0] == 0xFF && p[1] == 0xFF);
    CHECK(get_i16(p) == -1);
}

void test_framer() {
    section("Framer structure");
    Framer f;
    const auto frame = build_frame(f, make_sample());
    CHECK(frame.size() == kFrameSize);

    // SYNC word at the front, big-endian.
    CHECK(frame[0] == 0x1A && frame[1] == 0xCF && frame[2] == 0xFC && frame[3] == 0x1D);
    // version_type and payload_len in the header.
    CHECK(frame[4] == kVersionType);
    CHECK(frame[7] == kPayloadSize);

    // Sequence number increments on each encode.
    CHECK(f.next_seq() == 1);
    build_frame(f, make_sample());
    CHECK(f.next_seq() == 2);
}

void test_decode_roundtrip() {
    section("Decoder round-trip (no noise)");
    Framer f;
    const Telemetry tx = make_sample();
    const auto frame = build_frame(f, tx);

    Decoder d;
    // Only the very last byte may complete the frame.
    bool completed_early = false;
    bool completed = false;
    for (size_t i = 0; i < frame.size(); ++i) {
        const bool r = d.feed(frame[i]);
        if (r && i + 1 < frame.size()) completed_early = true;
        if (r) completed = true;
    }
    CHECK(!completed_early);
    CHECK(completed);
    CHECK(d.stats().frames_valid == 1);
    CHECK(d.stats().frames_crc_err == 0);
    CHECK(same(d.last_packet(), tx));
}

void test_sync_hunt() {
    section("SYNC hunt skips leading garbage");
    Framer f;
    const Telemetry tx = make_sample();
    auto stream = std::vector<uint8_t>{0x00, 0xFF, 0x1A, 0x42, 0x7E, 0x1A, 0xCF};  // junk
    const auto frame = build_frame(f, tx);
    stream.insert(stream.end(), frame.begin(), frame.end());

    Decoder d;
    CHECK(feed_all(d, stream));
    CHECK(d.stats().frames_valid == 1);
    CHECK(same(d.last_packet(), tx));
}

void test_two_frames() {
    section("Two frames back-to-back");
    Framer f;
    Decoder d;
    auto a = build_frame(f, make_sample());
    auto b = build_frame(f, make_sample());
    a.insert(a.end(), b.begin(), b.end());

    CHECK(feed_all(d, a));
    CHECK(d.stats().frames_valid == 2);
    CHECK(d.stats().frames_crc_err == 0);
    CHECK(d.last_seq() == 1);  // second frame carried sequence 1
}

void test_crc_catches_payload_flip() {
    section("CRC catches a corrupted payload");
    Framer f;
    auto frame = build_frame(f, make_sample());
    frame[10] ^= 0x01;  // flip one bit inside the payload

    Decoder d;
    CHECK(!feed_all(d, frame));
    CHECK(d.stats().frames_valid == 0);
    CHECK(d.stats().frames_crc_err == 1);
}

void test_crc_catches_crc_flip() {
    section("CRC catches a corrupted CRC field");
    Framer f;
    auto frame = build_frame(f, make_sample());
    frame[kFrameSize - 1] ^= 0x80;  // flip a bit in the CRC itself

    Decoder d;
    CHECK(!feed_all(d, frame));
    CHECK(d.stats().frames_crc_err == 1);
}

void test_resync_after_truncation() {
    section("Resync after a truncated frame");
    Framer f;
    auto good = build_frame(f, make_sample());

    // A frame cut off halfway, followed by some junk, then a whole frame.
    std::vector<uint8_t> stream(good.begin(), good.begin() + 11);
    stream.push_back(0x55);
    stream.push_back(0xAA);
    auto good2 = build_frame(f, make_sample());
    stream.insert(stream.end(), good2.begin(), good2.end());

    Decoder d;
    CHECK(feed_all(d, stream));     // the complete frame still decodes
    CHECK(d.stats().frames_valid == 1);
}

void test_bad_header_rejected() {
    section("Malformed header is rejected");
    // Hand-build a frame with a bad version byte, then a good frame after it.
    Framer f;
    auto good = build_frame(f, make_sample());

    std::vector<uint8_t> stream;
    // SYNC
    stream.insert(stream.end(), {0x1A, 0xCF, 0xFC, 0x1D});
    stream.push_back(0x02);            // wrong version (expected 0x01)
    stream.insert(stream.end(), {0x00, 0x00});  // seq
    stream.push_back(kPayloadSize);    // len
    for (size_t i = 0; i < kPayloadSize + 2; ++i) stream.push_back(0x00);  // filler

    stream.insert(stream.end(), good.begin(), good.end());

    Decoder d;
    CHECK(feed_all(d, stream));
    CHECK(d.stats().resyncs >= 1);       // the bad header forced a resync
    CHECK(d.stats().frames_valid == 1);  // the good frame still got through
}

void test_sequence_gap() {
    section("Sequence-gap detection");
    Framer f;
    Decoder d;

    auto f0 = build_frame(f, make_sample());  // seq 0
    build_frame(f, make_sample());            // seq 1 (dropped, never sent)
    build_frame(f, make_sample());            // seq 2 (dropped, never sent)
    auto f3 = build_frame(f, make_sample());  // seq 3

    CHECK(feed_all(d, f0));
    CHECK(feed_all(d, f3));
    CHECK(d.stats().frames_valid == 2);
    CHECK(d.stats().frames_dropped == 2);  // seq 1 and 2 are missing
}

void test_torture() {
    section("Torture: 1,000,000 random bytes");
    Decoder d;
    std::mt19937 rng(0xC0FFEE);
    for (int i = 0; i < 1'000'000; ++i) {
        d.feed(static_cast<uint8_t>(rng() & 0xFF));
    }
    // It must not have crashed or wedged: the decoder is still functional and a
    // real frame fed afterward decodes correctly.
    CHECK(d.stats().bytes_seen == 1'000'000);

    Framer f;
    const Telemetry tx = make_sample();
    const auto frame = build_frame(f, tx);
    CHECK(feed_all(d, frame));
    CHECK(same(d.last_packet(), tx));
}

}  // namespace

int main() {
    g_color = isatty(STDOUT_FILENO);

    std::printf("\n%s%s CubeSat Telemetry Link -- test suite %s\n\n",
                bold(), cyan(), reset());

    test_crc();
    test_serialization();
    test_framer();
    test_decode_roundtrip();
    test_sync_hunt();
    test_two_frames();
    test_crc_catches_payload_flip();
    test_crc_catches_crc_flip();
    test_resync_after_truncation();
    test_bad_header_rejected();
    test_sequence_gap();
    test_torture();

    std::printf("\n");
    if (g_fails == 0) {
        std::printf("%s%s  ALL %d CHECKS PASSED  %s\n\n", bold(), green(),
                    g_checks, reset());
        return 0;
    }
    std::printf("%s%s  %d/%d CHECKS FAILED  %s\n\n", bold(), red(), g_fails,
                g_checks, reset());
    return 1;
}
