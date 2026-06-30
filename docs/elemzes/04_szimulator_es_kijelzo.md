# 04 — A szimulátor és a kijelző: satellite és dashboard

A `Satellite` élethű telemetriát gyárt, a `Dashboard` pedig kirajzolja egy élő
ANSI-terminálra. Itt jön elő sok lambda (00/12), a varargs (00/17) és a látható-
szélesség igazítási trükk.

---

## 1. `include/satellite.hpp`

```cpp
#pragma once
#include <cstdint>
#include <random>
#include "protocol.hpp"

namespace satlink {

class Satellite {
public:
    explicit Satellite(uint32_t seed = 1);
    Telemetry sample(uint32_t t_ms);

private:
    std::mt19937                           rng_;
    std::uniform_real_distribution<double> noise_{-1.0, 1.0};
};

}
```

- `explicit Satellite(uint32_t seed = 1);` — a konstruktor.
  - `= 1` az **alapértelmezett argumentum**: `Satellite s;` esetén a mag 1 lesz.
  - `explicit` — megtiltja, hogy egy `uint32_t` **véletlenül** `Satellite`-tá
    konvertálódjon (pl. egy függvényhívásnál). Egyparaméteres konstruktoroknál jó
    szokás, nehogy rejtett konverzió történjen.
- `Telemetry sample(uint32_t t_ms);` — adott küldetési időre (ms) ad egy mintát.
- `rng_`, `noise_{-1.0, 1.0}` — a véletlenmotor és egy [−1,1] eloszlás a mérési
  zajhoz (00/14), kapcsos taginiciálással (00/6).

---

## 2. `src/satellite.cpp`

```cpp
#include "satellite.hpp"
#include <cmath>

namespace satlink {

namespace {
constexpr double kPi = 3.14159265358979323846;

double wave(double t_s, double period_s, double center, double amp,
            double phase = 0.0) {
    return center + amp * std::sin(2.0 * kPi * t_s / period_s + phase);
}
}  // namespace
```

- `#include <cmath>` — a `std::sin` és társai.
- `namespace { ... }` — **anonim névtér** (00/7): a `kPi` és a `wave` csak ebben a
  `.cpp`-ben látszik, nem szivárog ki.
- `constexpr double kPi = ...` — fordítási idejű π (00/3).
- `wave(...)` — egy segédfüggvény: szinuszhullám `period_s` periódussal, `center`
  középérték körül, `±amp` amplitúdóval, opcionális `phase` fáziseltolással.
  - `double phase = 0.0` — alapértelmezett argumentum: ha nem adod meg, 0.
  - `std::sin(2π·t/period + phase)` — a klasszikus szinusz; a `2π/period` a
    körfrekvencia.

```cpp
Satellite::Satellite(uint32_t seed) : rng_(seed) {}
```

- A konstruktor a motort a maggal építi (taginiciáló lista, 00/8). A `noise_`-t
  nem említi, mert annak a fejlécben adtunk alapértéket.
- Figyeld: a fejlécben volt `= 1` alapérték; a **definícióban** nem ismételjük meg
  (a nyelv tiltja a kétszeri megadást).

```cpp
Telemetry Satellite::sample(uint32_t t_ms) {
    const double t = t_ms / 1000.0;
    Telemetry tm;
    tm.timestamp_ms = t_ms;
```

- `const double t = t_ms / 1000.0;` — a milliszekundumot **másodperccé** váltjuk
  a hullámokhoz. A `1000.0` (nem `1000`!) miatt **lebegőpontos** osztás történik
  — ha `1000` lenne, egész osztás csonkítana. Apró, de fontos.
- `Telemetry tm;` majd a mezők feltöltése a „kézzel mezőnként" stílusban.

```cpp
    tm.batt_mv = static_cast<uint16_t>(
        wave(t, 40.0, 3700.0, 300.0) + 8.0 * noise_(rng_));
```

- Akkufeszültség: 40 s periódusú hullám, 3700 mV közép, ±300 mV, plusz `8.0 *
  noise_(rng_)` mérési zaj (a `noise_` egy [−1,1] minta, 00/14). A `static_cast<uint16_t>`
  a `double` eredményt a mező típusára szűkíti (csonkít a tört felé).

```cpp
    tm.temp_eps_c10 = static_cast<int16_t>(
        wave(t, 30.0, 200.0, 150.0) + 2.0 * noise_(rng_));
    tm.temp_obc_c10 = static_cast<int16_t>(
        wave(t, 30.0, 180.0, 120.0, -0.6) + 2.0 * noise_(rng_));
```

- Két hőmérséklet, 30 s periódussal. A `200.0` közép = 20.0 °C (mert `c10` =
  0.1 °C lépés). Az OBC fáziskésésben van (`-0.6` fázis) és kissé hűvösebb.
  `int16_t`, mert lehet negatív.

```cpp
    const double deg = wave(t, 25.0, 0.0, 180.0);
    tm.attitude_cdeg = static_cast<int16_t>(deg * 100.0);
```

- Helyzetszög: 25 s periódusú, 0 közép, ±180° hullám. A `deg * 100.0` váltja
  centifokra (00/3-as döntés: így fér el `int16_t`-ben).

```cpp
    if (tm.batt_mv < 3450) {
        tm.mode = Mode::Safe;
    } else {
        switch ((t_ms / 8000) % 3) {
            case 0:  tm.mode = Mode::Nominal; break;
            case 1:  tm.mode = Mode::Payload; break;
            default: tm.mode = Mode::Comms;   break;
        }
    }
    return tm;
}
```

- **Üzemmód a tápállapot szerint:** ha az akku < 3450 mV → `Safe`. Különben a
  `(t_ms / 8000) % 3` 8 másodpercenként váltja a módot 0→1→2 között. A `switch`
  egész értékre megy (nem enumra itt), `break`-ekkel.
- `return tm;` — kész a minta (érték szerint visszaadva).

---

## 3. `include/dashboard.hpp`

```cpp
class Dashboard {
public:
    explicit Dashboard(bool color_enabled = true) : color_(color_enabled) {}
    void begin();
    void end();
    void render(const Telemetry& tm, bool have_tm, const DecoderStats& stats,
                double ber, uint64_t flipped_bits, uint64_t total_bits,
                double elapsed_s);
private:
    bool color_;
};
```

- `explicit Dashboard(bool color_enabled = true) : color_(color_enabled) {}` — a
  konstruktor **a fejlécben**, inline, taginiciáló listával. Eltárolja, kell-e
  szín.
- `begin()` / `end()` — terminál-előkészítés/visszaállítás.
- `render(...)` — sok paraméter: a telemetria (`const&`), van-e már telemetria
  (`have_tm`), a statisztika (`const&`), a hibaarány, a bit-számlálók, az eltelt
  idő. Mind **bemenet**; a dashboard nem módosít semmit, csak rajzol.
- `bool color_;` — az egyetlen állapot: színesen rajzoljon-e.

---

## 4. `src/dashboard.cpp` — a belső eszközök

### ANSI-konstansok és elrendezés

```cpp
namespace {
constexpr const char* kReset   = "\x1b[0m";
constexpr const char* kBold    = "\x1b[1m";
...
constexpr int kInner = 62;
constexpr int kLabelCol = 2;
constexpr int kValueCol = 15;
constexpr int kBarCol   = 28;
constexpr int kNoteCol  = 49;
constexpr int kBarWidth = 18;
```

- Az egész belső konyha **anonim névtérben** (00/7).
- `constexpr const char* kReset = "\x1b[0m";` — egy **fordítási idejű pointer egy
  konstans sztringre**. A `"\x1b[0m"` egy ANSI escape-szekvencia: a `\x1b` az ESC
  karakter (27), a `[0m` a „minden formázás vissza". `[1m` = félkövér, `[31m` =
  piros, `[32m` = zöld, stb. Ezeket a terminál értelmezi színként/stílusként.
- `kInner = 62` — a doboz belső szélessége **látható oszlopban**.
- `kLabelCol`, `kValueCol`, ... — **abszolút oszloppozíciók**, ahova igazítunk.
  Ez a kulcs a szép igazításhoz (lásd a `Row`-t lentebb).

### Apró segédek

```cpp
double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }
```

- `clamp01` — egy számot a [0,1] tartományba szorít. Háromtagú `?:` operátor
  (00/21), kétszer láncolva. A mértékeknél (gauge) kell, nehogy túlcsorduljon a
  sáv.

```cpp
std::string fmt(const char* f, ...) {
    char buf[96];
    va_list ap; va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}
```

- A printf-burkoló, ami **sztringet** ad vissza (00/17). Egy 96 bájtos
  veremtömbbe formáz biztonságosan (`vsnprintf`, sosem túlír), majd `std::string`-
  ként visszaadja. Ezzel kényelmes számokat formázni: `fmt("%.2f V", v)`.

```cpp
std::string repeat(const char* unit, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += unit;
    return s;
}
```

- Egy (akár többbájtos) jelet `n`-szer ismétel. A dobozkeret `═` vonalához kell:
  `repeat("═", 62)`. A `+=` sztringhez fűz (00/13).

```cpp
std::string make_bar(double frac, int width) {
    const int filled = static_cast<int>(std::lround(clamp01(frac) * width));
    std::string s;
    for (int i = 0; i < width; ++i) s += (i < filled ? "█" : "░");
    return s;
}
```

- A **mérce** (gauge): `width` cellából az első `frac` hányad „tele".
  - `std::lround(clamp01(frac) * width)` — a kitöltött cellák száma kerekítve
    (`<cmath>`); a `clamp01` véd a túlcsordulástól.
  - A ciklus `█` (tele blokk) vagy `░` (üres blokk) karaktert fűz. Ezek
    **többbájtos** UTF-8 jelek, de **1 oszlop** szélesek — ezt a hívónál kézzel
    számoljuk (lásd `glyph`).

### A `Row` — egy doboz-sor építője (a szélesség-trükk)

```cpp
struct Row {
    bool        color;
    std::string buf;
    int         vis = 0;

    void asc(const std::string& s) { buf += s; vis += static_cast<int>(s.size()); }
    void glyph(const std::string& s, int w) { buf += s; vis += w; }
    void ansi(const char* code) { if (color) buf += code; }
    void pad(int target) { while (vis < target) { buf += ' '; ++vis; } }

    void value(const char* code, const std::string& s) {
        ansi(code); asc(s); ansi(kReset);
    }
    void gauge(const char* code, double frac) {
        ansi(code); asc("["); glyph(make_bar(frac, kBarWidth), kBarWidth); asc("]"); ansi(kReset);
    }
};
```

Ez a dashboard **szíve**. A probléma: a sornak pontosan `kInner` (62) **látható**
oszlop szélesnek kell lennie, hogy a jobb keret igazodjon. De a sztring **bájt**-
hossza nem egyenlő az oszlopszámmal (a `°`, `█`, `✓` többbájtos). Ezért a `Row`
**külön** számolja a látható szélességet (`vis`), miközben épül a bájtpuffer (`buf`):

- `bool color;` — örökli a `Dashboard.color_`-t (kell-e ANSI-kód).
- `std::string buf;` — a felépülő **bájtok** (ANSI-kódokkal, UTF-8-cal együtt).
- `int vis = 0;` — az eddigi **látható oszlopok** száma (00/6 alapérték).
- `asc(s)` — **ASCII** szöveg: a bájthossz **egyenlő** az oszlopszámmal, ezért
  `vis += s.size()`. (Csak tiszta ASCII-ra használjuk!)
- `glyph(s, w)` — egy jel, aminek a látható szélességét **kézzel** megadjuk
  (`w`). Így a `°`-t `glyph("°", 1)`-ként, a 18 cellás sávot `glyph(bar, 18)`-ként
  tesszük be — a `vis` helyesen nő, bár a `buf` több bájttal.
- `ansi(code)` — egy **nulla szélességű** ANSI-kód; csak ha `color` igaz. A `vis`
  nem nő (a kód nem látszik a képernyőn).
- `pad(target)` — szóközökkel tölt az **abszolút** `target` oszlopig. Ez teszi
  lehetővé az oszlopos igazítást: nem relatív tabokkal, hanem „told ki a 15.
  oszlopig".
- `value(code, s)` — egy **színes ASCII érték**: szín + szöveg + reset. A
  szélesség pontos, mert csak az `asc(s)` számít a `vis`-be.
- `gauge(code, frac)` — színes mérce szögletes zárójelben: `[` + 18 cellás sáv +
  `]`. A sáv `glyph`-fel megy be (helyes szélesség).

**Ez a hiba, amit fejlesztés közben elkaptunk:** eredetileg a `✓`/`✗` jeleket
`value()`-vel (azaz `asc()`-vel) tettük be — az a **bájthosszt** (3) adta a
`vis`-hez 1 helyett, így a sor 2 oszloppal rövidült. A javítás: `glyph(mark, 1)`.
Pont ezt tanítja ez a struktúra: UTF-8-nál sosem a bájthossz a látható szélesség.

### Mód- és linkszínek

```cpp
const char* mode_name(Mode m) {
    switch (m) {
        case Mode::Safe:    return "SAFE";
        ...
    }
    return "UNKNOWN";
}
const char* mode_color(Mode m) { ... return kRed; }
```

- `mode_name` / `mode_color` — egy `Mode`-ból nevet, illetve ANSI-színt ad. A
  `switch` lefedi mind a 4 értéket; a `return "UNKNOWN"` / `return kRed` az
  **érvénytelen** mode-ra véd (ha a bájt sérült, de átment a CRC-n — ritka).
- `const char*` visszatérés: konstans sztringre/kódra mutató pointer, nem másol.

```cpp
struct LinkInfo { const char* label; const char* color; double severity; };
LinkInfo link_info(double ber) {
    double sev = 0.0;
    if (ber > 0.0) sev = clamp01((std::log10(ber) + 6.0) / 5.0);
    ...
    return {label, col, sev};
}
```

- `LinkInfo` — egy kis struct három mezővel: címke, szín, súlyosság. „Több
  visszatérési érték" tiszta megoldása egy struct- tal.
- `sev` — a link súlyossága 0..1 között, **logaritmikus** skálán 1e-6 és 1e-1
  között (`std::log10`). Ez hajtja a link-mércét.
- `return {label, col, sev};` — **aggregátum-inicializálás** a return-ben (00/6):
  a `LinkInfo` három mezőjét a sorrendjükben tölti fel.

### `begin` és `end` — a terminál kezelése

```cpp
void Dashboard::begin() {
    std::fputs("\x1b[2J\x1b[?25l", stdout);
    std::fflush(stdout);
}
void Dashboard::end() {
    std::fputs("\x1b[?25h\n", stdout);
    std::fflush(stdout);
}
```

- `begin`: `\x1b[2J` = teljes képernyő törlése, `\x1b[?25l` = **kurzor
  elrejtése** (hogy ne villogjon a rajzolás közben). `fflush` = azonnali kiírás
  (00/17).
- `end`: `\x1b[?25h` = kurzor **visszaadása**, `\n` = új sor. Ezt a fő ciklus
  hívja kilépéskor, hogy a terminál ne maradjon „kurzor nélkül".

---

## 5. `src/dashboard.cpp` — a `render`

A `render` egy nagy függvény, de **ismétlődő minták** lambdákkal (00/12). A váz:

```cpp
void Dashboard::render(...) {
    std::string out;
    out.reserve(4096);
    out += "\x1b[H";
```

- `std::string out;` — az **egész képkockát** egyetlen sztringbe építjük, majd
  **egyszer** írjuk ki (kevesebb villogás). `reserve(4096)` előre foglal (00/13).
- `out += "\x1b[H";` — a kurzort a **bal felső sarokba** viszi. Így minden render
  felülírja az előzőt a helyén (nem görget).

```cpp
    const std::string top = "╔" + repeat("═", kInner) + "╗";
    const std::string sep = "╠" + repeat("═", kInner) + "╣";
    const std::string bot = "╚" + repeat("═", kInner) + "╝";
```

- A három **keretvonal** (felső, elválasztó, alsó), `kInner` (62) darab `═`
  között a megfelelő sarok/T-jelekkel. A `+` itt sztring-összefűzés (00/13).

```cpp
    auto rule = [&](const std::string& s) { out += s; out += "\x1b[K\n"; };
    auto emit = [&](Row& r) {
        r.pad(kInner);
        out += "║"; out += r.buf; out += "║\x1b[K\n";
    };
    auto new_row = [&] { Row r; r.color = color_; return r; };
```

Három **lambda** (00/12), mind `[&]` (látja a külső `out`-ot és `color_`-t):

- `rule(s)` — egy teljes vonalat (keret) kiír, a végén `\x1b[K` (sor végéig
  törlés, hogy ne maradjon szemét) + új sor.
- `emit(r)` — egy **tartalmi sort** zár le: a `Row`-t `kInner`-ig tölti (`pad`),
  majd `║ ... ║` keretbe teszi, `\x1b[K\n`-nel zár. Ez biztosítja, hogy minden
  sor pontosan 62 látható oszlop legyen a keretek között.
- `new_row()` — egy friss `Row`-t ad, a `color_` flaggel beállítva. (Capture `[&]`,
  paraméter nincs.)

A továbbiakban a `render` ezeket hívogatja, szakaszonként. Néhány jellemző
darab:

#### A cím

```cpp
    rule(top);
    {
        Row r = new_row();
        const std::string title = "CUBESAT TELEMETRY LINK   //   GROUND STATION";
        const int lead = (kInner - static_cast<int>(title.size())) / 2;
        r.pad(lead);
        r.ansi(kBold); r.ansi(kCyan);
        r.asc(title);
        r.ansi(kReset);
        emit(r);
    }
    rule(sep);
```

- `{ ... }` — egy **névtelen blokk**, csak hogy a `r`, `title`, `lead` lokális
  legyen ehhez a sorhoz (a következő sor újra deklarálhat `r`-t). Tiszta
  hatókör-kezelés.
- `lead = (kInner - title.size()) / 2` — a cím **középre** igazításához a bal
  oldali térköz. A `title.size()` itt ASCII, ezért a bájthossz = oszlopszám.
- `pad(lead)` → `bold`+`cyan` → `asc(title)` → `reset` → `emit`. A `bold`/`cyan`
  nulla szélességű, a `title` adja a `vis`-t, az `emit` tölti 62-ig.

#### Egy telemetria-sor (lambdával)

```cpp
    auto channel_row = [&](const std::string& label, bool ok,
                           const char* vcolor, const std::string& value,
                           double frac, const char* gcolor,
                           const std::string& note) {
        Row r = new_row();
        r.pad(kLabelCol);  r.asc(label);
        r.pad(kValueCol);  if (ok) r.value(vcolor, value); else r.value(kDim, "------");
        r.pad(kBarCol);    r.gauge(ok ? gcolor : kDim, frac);
        r.pad(kNoteCol);   r.ansi(kDim); r.asc(note); r.ansi(kReset);
        emit(r);
    };
```

- Egy **újrahasznosítható** sorsablon: címke a `kLabelCol`-tól, érték a
  `kValueCol`-tól, mérce a `kBarCol`-tól, megjegyzés a `kNoteCol`-tól. Az abszolút
  oszlopokhoz `pad`-elünk, így minden sor szépen egymás alá igazodik.
- `if (ok) ... else r.value(kDim, "------")` — ha még nincs telemetria
  (`have_tm == false`), halvány „------" helyőrző.

```cpp
    {
        const double v = tm.batt_mv / 1000.0;
        const char* col = tm.batt_mv >= 3600 ? kGreen
                          : tm.batt_mv >= 3450 ? kYellow : kRed;
        channel_row("Battery", have_tm, col, fmt("%.2f V", v),
                    (tm.batt_mv - 3400.0) / 600.0, col, "3.40-4.00 V");
    }
```

- Az akku-sor: `v` a feszültség V-ban, `col` a szín **küszöbök** szerint (láncolt
  `?:`, 00/21). A mérce hányada `(batt_mv - 3400) / 600` (a 3400–4000 mV
  tartományt 0..1-re képezi). A `fmt("%.2f V", v)` formázza az értéket.

A hőmérséklet- és helyzet-sorok hasonlók, de a `°` miatt **kézzel** építik az
értéket (`r.asc(fmt("%+.1f", c)); r.glyph("°", 1); r.asc("C");`) — itt látszik,
miért nem mehet a `°` az `asc`-on át.

#### A számlálók (a `✓`/`✗`-os rész)

```cpp
    auto counter_row = [&](const std::string& label, uint64_t n,
                           const char* col, const std::string& mark) {
        Row r = new_row();
        r.pad(kLabelCol);     r.asc(label);
        r.pad(kValueCol + 4); r.value(col, fmt("%llu", static_cast<unsigned long long>(n)));
        if (!mark.empty()) {
            r.pad(kBarCol);
            r.ansi(col); r.glyph(mark, 1); r.ansi(kReset);
        }
        emit(r);
    };
    counter_row("Frames valid", stats.frames_valid, kGreen, "✓");
    counter_row("CRC errors", stats.frames_crc_err,
                stats.frames_crc_err ? kRed : kDim, "✗");
```

- `fmt("%llu", static_cast<unsigned long long>(n))` — a `uint64_t`-t `%llu`-val
  formázzuk; a `static_cast<unsigned long long>` biztosítja, hogy a típus pontosan
  illeszkedjen a `%llu`-hoz (00/17). (A `uint64_t` és az `unsigned long long`
  rendszerenként ugyanaz, de a cast hordozhatóvá teszi.)
- `if (!mark.empty())` — csak akkor rajzol jelet, ha van (a „Dropped"/"Resyncs"
  soroknak nincs).
- `r.glyph(mark, 1)` — a `✓`/`✗` **1 oszlopként** megy be (a javított hiba).
- `stats.frames_crc_err ? kRed : kDim` — ha van CRC-hiba, **pirosan** villan,
  különben halvány. Vizuális jelzés.

#### Az áteresztés (frame yield)

```cpp
    {
        const uint64_t total = stats.frames_valid + stats.frames_crc_err;
        const double yield = total ? 100.0 * stats.frames_valid / total : 0.0;
        const char* col = yield >= 95 ? kGreen : yield >= 80 ? kYellow : kRed;
        ...
        r.gauge(col, yield / 100.0);
    }
```

- `total ? ... : 0.0` — **nullával osztás elleni védelem**: ha még nincs keret,
  0%. (A `total` mint feltétel: nem nulla → igaz.)
- `yield` a sikeres keretek százaléka, a mérce ezt mutatja. Ez a „bizonyíték":
  zajos linken esik, tisztán 100%.

#### A lezárás

```cpp
    rule(bot);
    out += "  ";
    if (color_) out += kDim;
    out += "Ctrl-C to stop";
    if (color_) out += kReset;
    out += "\x1b[K\n";

    std::fputs(out.c_str(), stdout);
    std::fflush(stdout);
}
```

- Alsó keret, majd egy lábléc a **dobozon kívül** (sima `out +=`, nem `Row`).
- `std::fputs(out.c_str(), stdout)` — a **teljes** képkockát egyszerre kiírja
  (`c_str()` a C-stílusú pointer, 00/13). `fflush` → azonnal látszik.

---

A megjelenítés kész. Az [05-ös fájl](05_fociklus_es_tesztek.md) a fő ciklust (ami
mindezt összeköti) és a tesztcsomagot elemzi.
