// dashboard.hpp — Live terminal dashboard (the ground-station display).
//
// Renders the decoded telemetry and the link/decoder statistics as a full-screen
// ANSI dashboard that refreshes in place. This is the "obvious result" of the
// project: you watch real values update, and you watch the CRC-error counter
// climb as the channel gets noisier while good frames still get through.

#pragma once

#include <cstdint>

#include "decoder.hpp"
#include "protocol.hpp"

namespace satlink {

class Dashboard {
public:
    explicit Dashboard(bool color_enabled = true) : color_(color_enabled) {}

    // Prepare the terminal: clear the screen and hide the cursor.
    void begin();
    // Restore the terminal: show the cursor again.
    void end();

    // Draw one full frame of the dashboard, overwriting the previous one.
    //   tm          : most recently decoded telemetry
    //   have_tm     : false until the first valid frame has been decoded
    //   stats       : decoder counters
    //   ber         : current channel bit error rate
    //   flipped/total bits : channel totals
    //   elapsed_s   : wall-clock seconds since the run started
    void render(const Telemetry& tm, bool have_tm, const DecoderStats& stats,
                double ber, uint64_t flipped_bits, uint64_t total_bits,
                double elapsed_s);

private:
    bool color_;
};

} // namespace satlink
