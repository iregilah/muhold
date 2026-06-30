// dashboard.cpp — implementation of the live ANSI terminal dashboard.

#include "dashboard.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace satlink {
namespace {

// --- ANSI escape sequences --------------------------------------------------
constexpr const char* kReset   = "\x1b[0m";
constexpr const char* kBold    = "\x1b[1m";
constexpr const char* kDim     = "\x1b[2m";
constexpr const char* kRed     = "\x1b[31m";
constexpr const char* kGreen   = "\x1b[32m";
constexpr const char* kYellow  = "\x1b[33m";
constexpr const char* kCyan    = "\x1b[36m";
constexpr const char* kMagenta = "\x1b[35m";

// Inner width of the box, in visible columns (between the two border bars).
constexpr int kInner = 62;

// Column positions (visible) used to line everything up. Padding to an absolute
// column keeps alignment correct even though some glyphs (the degree sign, the
// gauge blocks, the check marks) are multiple UTF-8 bytes but one column wide.
constexpr int kLabelCol = 2;
constexpr int kValueCol = 15;
constexpr int kBarCol   = 28;
constexpr int kNoteCol  = 49;
constexpr int kBarWidth = 18;

double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

// printf into a std::string.
std::string fmt(const char* f, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}

// Repeat a (possibly multi-byte) glyph `n` times.
std::string repeat(const char* unit, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += unit;
    return s;
}

// A horizontal gauge: `width` cells, the first `frac` fraction filled.
std::string make_bar(double frac, int width) {
    const int filled = static_cast<int>(std::lround(clamp01(frac) * width));
    std::string s;
    for (int i = 0; i < width; ++i) s += (i < filled ? "█" : "░"); // full / light block
    return s;
}

// One bordered content line, built segment by segment while tracking the
// visible column so multi-byte glyphs do not throw off the alignment.
struct Row {
    bool        color;
    std::string buf;
    int         vis = 0;

    // Plain ASCII text (length in bytes equals width in columns).
    void asc(const std::string& s) { buf += s; vis += static_cast<int>(s.size()); }
    // A glyph whose visible width we state explicitly (e.g. a multi-byte block).
    void glyph(const std::string& s, int w) { buf += s; vis += w; }
    // A zero-width ANSI code; emitted only when colour is enabled.
    void ansi(const char* code) { if (color) buf += code; }
    // Pad with spaces up to absolute column `target`.
    void pad(int target) { while (vis < target) { buf += ' '; ++vis; } }

    // A coloured ASCII value: code + text + reset, all at column-accurate width.
    void value(const char* code, const std::string& s) {
        ansi(code);
        asc(s);
        ansi(kReset);
    }
    // A coloured gauge with brackets.
    void gauge(const char* code, double frac) {
        ansi(code);
        asc("[");
        glyph(make_bar(frac, kBarWidth), kBarWidth);
        asc("]");
        ansi(kReset);
    }
};

const char* mode_name(Mode m) {
    switch (m) {
        case Mode::Safe:    return "SAFE";
        case Mode::Nominal: return "NOMINAL";
        case Mode::Payload: return "PAYLOAD";
        case Mode::Comms:   return "COMMS";
    }
    return "UNKNOWN";
}

const char* mode_color(Mode m) {
    switch (m) {
        case Mode::Safe:    return kYellow;
        case Mode::Nominal: return kGreen;
        case Mode::Payload: return kCyan;
        case Mode::Comms:   return kMagenta;
    }
    return kRed;
}

// Map a bit error rate to a (label, colour, severity) triple. Severity is a
// 0..1 value driving the link gauge, on a log scale between 1e-6 and 1e-1.
struct LinkInfo { const char* label; const char* color; double severity; };
LinkInfo link_info(double ber) {
    double sev = 0.0;
    if (ber > 0.0) sev = clamp01((std::log10(ber) + 6.0) / 5.0);  // 1e-6..1e-1 -> 0..1
    const char* label;
    const char* col;
    if      (ber <= 1e-6) { label = "CLEAN";    col = kGreen;  }
    else if (ber <= 1e-4) { label = "GOOD";     col = kGreen;  }
    else if (ber <= 1e-3) { label = "DEGRADED"; col = kYellow; }
    else if (ber <= 1e-2) { label = "POOR";     col = kRed;    }
    else                  { label = "CRITICAL"; col = kRed;    }
    return {label, col, sev};
}

}  // namespace

void Dashboard::begin() {
    std::fputs("\x1b[2J\x1b[?25l", stdout);  // clear screen, hide cursor
    std::fflush(stdout);
}

void Dashboard::end() {
    std::fputs("\x1b[?25h\n", stdout);       // show cursor again
    std::fflush(stdout);
}

void Dashboard::render(const Telemetry& tm, bool have_tm,
                       const DecoderStats& stats, double ber,
                       uint64_t flipped_bits, uint64_t total_bits,
                       double elapsed_s) {
    std::string out;
    out.reserve(4096);

    // Move cursor home; each line is cleared to end-of-line as we draw it.
    out += "\x1b[H";

    const std::string top = "╔" + repeat("═", kInner) + "╗";
    const std::string sep = "╠" + repeat("═", kInner) + "╣";
    const std::string bot = "╚" + repeat("═", kInner) + "╝";

    auto rule = [&](const std::string& s) { out += s; out += "\x1b[K\n"; };
    auto emit = [&](Row& r) {
        r.pad(kInner);
        out += "║";
        out += r.buf;
        out += "║\x1b[K\n";
    };
    auto new_row = [&] { Row r; r.color = color_; return r; };

    // --- Title ---------------------------------------------------------------
    rule(top);
    {
        Row r = new_row();
        const std::string title = "CUBESAT TELEMETRY LINK   //   GROUND STATION";
        const int lead = (kInner - static_cast<int>(title.size())) / 2;
        r.pad(lead);
        r.ansi(kBold);
        r.ansi(kCyan);
        r.asc(title);
        r.ansi(kReset);
        emit(r);
    }
    rule(sep);

    // --- Mission time and mode ----------------------------------------------
    {
        const int secs = static_cast<int>(elapsed_s);
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc("MISSION TIME");
        r.pad(kValueCol);
        r.value(kBold, fmt("%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60));
        r.pad(kNoteCol - 14);
        r.asc("MODE");
        r.pad(kNoteCol - 4);
        if (have_tm) r.value(mode_color(tm.mode), mode_name(tm.mode));
        else         r.value(kDim, "----");
        emit(r);
    }

    // --- A blank spacer line -------------------------------------------------
    { Row r = new_row(); emit(r); }

    // --- Telemetry channels --------------------------------------------------
    auto channel_row = [&](const std::string& label, bool ok,
                           const char* vcolor, const std::string& value,
                           double frac, const char* gcolor,
                           const std::string& note) {
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc(label);
        r.pad(kValueCol);
        if (ok) r.value(vcolor, value);
        else    r.value(kDim, "------");
        r.pad(kBarCol);
        r.gauge(ok ? gcolor : kDim, frac);
        r.pad(kNoteCol);
        r.ansi(kDim);
        r.asc(note);
        r.ansi(kReset);
        emit(r);
    };

    // Battery
    {
        const double v = tm.batt_mv / 1000.0;
        const char* col = tm.batt_mv >= 3600 ? kGreen
                          : tm.batt_mv >= 3450 ? kYellow : kRed;
        channel_row("Battery", have_tm, col, fmt("%.2f V", v),
                    (tm.batt_mv - 3400.0) / 600.0, col, "3.40-4.00 V");
    }
    // EPS / OBC temperatures (value carries a degree sign, so build it by hand)
    auto temp_row = [&](const std::string& label, int16_t c10) {
        const double c = c10 / 10.0;
        const char* col = (c >= 5 && c <= 35) ? kGreen
                          : (c >= -10 && c <= 45) ? kYellow : kRed;
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc(label);
        r.pad(kValueCol);
        if (have_tm) {
            r.ansi(col);
            r.asc(fmt("%+.1f", c));
            r.glyph("°", 1);  // degree sign
            r.asc("C");
            r.ansi(kReset);
        } else {
            r.value(kDim, "------");
        }
        r.pad(kBarCol);
        r.gauge(have_tm ? col : kDim, (c + 10.0) / 55.0);  // -10..45 C
        emit(r);
    };
    temp_row("EPS temp", tm.temp_eps_c10);
    temp_row("OBC temp", tm.temp_obc_c10);
    // Attitude
    {
        const double deg = tm.attitude_cdeg / 100.0;
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc("Attitude");
        r.pad(kValueCol);
        if (have_tm) {
            r.ansi(kCyan);
            r.asc(fmt("%+.1f", deg));
            r.glyph("°", 1);
            r.ansi(kReset);
        } else {
            r.value(kDim, "------");
        }
        r.pad(kBarCol);
        r.gauge(have_tm ? kCyan : kDim, (deg + 180.0) / 360.0);
        r.pad(kNoteCol);
        r.ansi(kDim);
        r.asc("-180..+180");
        r.ansi(kReset);
        emit(r);
    }

    // --- Link section --------------------------------------------------------
    rule(sep);
    {
        Row r = new_row();
        r.pad(kLabelCol);
        r.ansi(kBold);
        r.asc("LINK");
        r.ansi(kReset);
        emit(r);
    }
    {
        const LinkInfo li = link_info(ber);
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc("Bit error rate");
        r.pad(kValueCol + 2);
        r.value(li.color, fmt("%.1e", ber));
        r.pad(kBarCol);
        r.gauge(li.color, li.severity);
        r.pad(kNoteCol);
        r.value(li.color, li.label);
        emit(r);
    }
    {
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc("Bits flipped");
        r.pad(kValueCol + 2);
        r.asc(fmt("%llu / %llu",
                  static_cast<unsigned long long>(flipped_bits),
                  static_cast<unsigned long long>(total_bits)));
        emit(r);
    }

    // --- Decoder section -----------------------------------------------------
    rule(sep);
    {
        Row r = new_row();
        r.pad(kLabelCol);
        r.ansi(kBold);
        r.asc("DECODER");
        r.ansi(kReset);
        emit(r);
    }
    auto counter_row = [&](const std::string& label, uint64_t n,
                           const char* col, const std::string& mark) {
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc(label);
        r.pad(kValueCol + 4);
        r.value(col, fmt("%llu", static_cast<unsigned long long>(n)));
        if (!mark.empty()) {
            r.pad(kBarCol);
            r.ansi(col);
            r.glyph(mark, 1);  // check/cross marks are multi-byte but 1 column
            r.ansi(kReset);
        }
        emit(r);
    };
    counter_row("Frames valid", stats.frames_valid, kGreen, "✓");  // check
    counter_row("CRC errors", stats.frames_crc_err,
                stats.frames_crc_err ? kRed : kDim, "✗");          // cross
    counter_row("Dropped (gaps)", stats.frames_dropped, kYellow, "");
    counter_row("Resyncs", stats.resyncs, kYellow, "");
    {
        const uint64_t total = stats.frames_valid + stats.frames_crc_err;
        const double yield = total ? 100.0 * stats.frames_valid / total : 0.0;
        const char* col = yield >= 95 ? kGreen : yield >= 80 ? kYellow : kRed;
        Row r = new_row();
        r.pad(kLabelCol);
        r.asc("Frame yield");
        r.pad(kValueCol + 4);
        r.value(col, fmt("%.1f %%", yield));
        r.pad(kBarCol);
        r.gauge(col, yield / 100.0);
        emit(r);
    }

    rule(bot);

    // Footer (outside the box).
    out += "  ";
    if (color_) out += kDim;
    out += "Ctrl-C to stop";
    if (color_) out += kReset;
    out += "\x1b[K\n";

    std::fputs(out.c_str(), stdout);
    std::fflush(stdout);
}

} // namespace satlink
