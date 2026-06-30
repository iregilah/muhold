// main.cpp — the end-to-end simulation loop.
//
// This wires the whole pipeline together, exactly the way a ground station's
// software would sit at the end of a real radio link:
//
//   Satellite.sample()  ->  Framer.encode()  ->  NoisyChannel.transmit()
//        ->  Decoder.feed() one byte at a time  ->  Dashboard.render()
//
// Run it with no arguments for a self-driving demo whose link quality sweeps
// from clean to noisy and back, so you can watch good frames keep decoding
// while corrupted ones get caught by the CRC.

#include <unistd.h>  // isatty, STDOUT_FILENO

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>

#include "channel.hpp"
#include "dashboard.hpp"
#include "decoder.hpp"
#include "framer.hpp"
#include "protocol.hpp"
#include "satellite.hpp"

using namespace satlink;

namespace {

// Set by the SIGINT handler so the main loop can exit cleanly and restore the
// terminal (sig_atomic_t is the only type safe to touch from a signal handler).
volatile std::sig_atomic_t g_stop = 0;
void on_sigint(int) { g_stop = 1; }

// The default self-driving link-quality schedule: a 32-second cycle that steps
// from a clean link through increasingly noisy conditions and back. Returning
// the BER as a function of time keeps the demo reproducible.
double ber_sweep(double t_s) {
    const double phase = std::fmod(t_s, 32.0);
    if (phase < 8.0)  return 0.0;    // CLEAN: nothing should fail
    if (phase < 16.0) return 1e-4;   // GOOD: rare errors
    if (phase < 24.0) return 1e-3;   // DEGRADED: CRC starts catching frames
    return 5e-3;                     // POOR: many frames rejected
}

struct Options {
    double   ber       = -1.0;   // <0 means "use the automatic sweep"
    uint32_t seed      = 1;
    int      rate      = 8;      // telemetry frames per second
    double   duration  = 0.0;    // 0 means run until Ctrl-C
    bool     once      = false;  // render a single snapshot and exit
    bool     color     = true;
    bool     garbage   = true;   // inject line noise between frames
};

void print_usage(const char* prog) {
    std::printf(
        "CubeSat Telemetry Link Simulator\n"
        "Usage: %s [options]\n\n"
        "  --ber <rate>     fixed bit error rate (e.g. 1e-3); default: auto-sweep\n"
        "  --seed <n>       RNG seed for reproducible noise (default 1)\n"
        "  --rate <n>       telemetry frames per second (default 8)\n"
        "  --duration <s>   stop after s simulated seconds (default: run forever)\n"
        "  --once           render one snapshot frame and exit\n"
        "  --no-color       disable ANSI colours\n"
        "  --no-garbage     do not inject inter-frame line noise\n"
        "  --help           show this help\n",
        prog);
}

// Tiny argument parser. Returns false if the program should exit immediately.
bool parse_args(int argc, char** argv, Options& opt, int& exit_code) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto need_value = [&](double& dst) {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a); exit_code = 2; return false; }
            dst = std::atof(argv[++i]);
            return true;
        };
        if (!std::strcmp(a, "--ber")) { if (!need_value(opt.ber)) return false; }
        else if (!std::strcmp(a, "--seed")) { double v; if (!need_value(v)) return false; opt.seed = static_cast<uint32_t>(v); }
        else if (!std::strcmp(a, "--rate")) { double v; if (!need_value(v)) return false; opt.rate = v > 0 ? static_cast<int>(v) : 1; }
        else if (!std::strcmp(a, "--duration")) { if (!need_value(opt.duration)) return false; }
        else if (!std::strcmp(a, "--once")) { opt.once = true; }
        else if (!std::strcmp(a, "--no-color")) { opt.color = false; }
        else if (!std::strcmp(a, "--no-garbage")) { opt.garbage = false; }
        else if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) { print_usage(argv[0]); exit_code = 0; return false; }
        else { std::fprintf(stderr, "unknown option: %s\n", a); print_usage(argv[0]); exit_code = 2; return false; }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    int exit_code = 0;
    if (!parse_args(argc, argv, opt, exit_code)) return exit_code;

    // If stdout is not a terminal (piped to a file), colour and cursor tricks
    // would just add noise, so turn them off automatically.
    if (!isatty(STDOUT_FILENO)) opt.color = false;

    // For a one-shot snapshot we want some visible CRC activity, so default to a
    // degraded link unless the user pinned a specific BER.
    if (opt.once && opt.ber < 0.0) opt.ber = 1e-3;

    std::signal(SIGINT, on_sigint);

    Satellite    satellite(opt.seed);
    Framer       framer;
    NoisyChannel channel(opt.ber < 0.0 ? 0.0 : opt.ber, opt.seed ^ 0xA5A5u);
    Decoder      decoder;
    Dashboard    dashboard(opt.color);
    std::mt19937 noise_rng(opt.seed ^ 0x1234u);

    // Snapshot mode produces a fixed number of frames quickly, then draws once.
    const uint64_t snapshot_frames = static_cast<uint64_t>(opt.rate) * 8;

    Telemetry last_decoded{};
    bool       have_decoded = false;

    if (!opt.once) dashboard.begin();

    uint64_t frame_idx = 0;
    uint8_t  frame[kFrameSize];

    while (!g_stop) {
        // Mission time advances at the telemetry rate (decoupled from any
        // scheduling jitter), so telemetry and the clock stay reproducible.
        const uint32_t t_ms = static_cast<uint32_t>(frame_idx * 1000 / opt.rate);
        const double   t_s  = t_ms / 1000.0;

        const double ber = (opt.ber < 0.0) ? ber_sweep(t_s) : opt.ber;
        channel.set_bit_error_rate(ber);

        // 1) Generate a telemetry sample and 2) frame it.
        const Telemetry tm = satellite.sample(t_ms);
        const size_t n = framer.encode(tm, frame, sizeof(frame));

        // Optionally feed a few random "line noise" bytes ahead of the frame so
        // the decoder's SYNC hunt and resync are exercised live.
        if (opt.garbage && (noise_rng() % 100) < 12) {
            const int g = 1 + static_cast<int>(noise_rng() % 3);
            for (int k = 0; k < g; ++k) {
                decoder.feed(static_cast<uint8_t>(noise_rng() & 0xFF));
            }
        }

        // 3) Corrupt the frame on the channel, then 4) feed it to the decoder
        //    one byte at a time, just like a UART delivering received bytes.
        channel.transmit(frame, n);
        for (size_t k = 0; k < n; ++k) {
            if (decoder.feed(frame[k])) {
                last_decoded = decoder.last_packet();
                have_decoded = true;
            }
        }

        // 5) Draw the dashboard (every frame in live mode).
        if (!opt.once) {
            dashboard.render(last_decoded, have_decoded, decoder.stats(), ber,
                             channel.flipped_bits(), channel.total_bits(), t_s);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1000 / opt.rate));
        }

        ++frame_idx;

        if (opt.once && frame_idx >= snapshot_frames) break;
        if (!opt.once && opt.duration > 0.0 && t_s >= opt.duration) break;
    }

    if (opt.once) {
        const double t_s = static_cast<double>(frame_idx) / opt.rate;
        dashboard.render(last_decoded, have_decoded, decoder.stats(), opt.ber,
                         channel.flipped_bits(), channel.total_bits(), t_s);
    } else {
        dashboard.end();
    }

    return 0;
}
