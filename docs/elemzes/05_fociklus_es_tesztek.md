# 05 — A fő ciklus, a tesztek és a Makefile

Itt áll össze minden: a `main.cpp` a csövet hajtja, a `test_main.cpp` bizonyítja
a helyességet, a `Makefile` fordít. A nyelvi elemekhez a
[00-s alapozó](00_nyelvi_alapozo.md).

---

## 1. `src/main.cpp`

### Include-ok

```cpp
#include <unistd.h>   // isatty, STDOUT_FILENO
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
```

- `<unistd.h>` — POSIX, az `isatty`-hoz (00/20).
- `<chrono>`, `<thread>` — időzítés és alvás (00/15).
- `<csignal>` — a `SIGINT` kezeléshez (00/19).
- `<cstdlib>` — az `atof`/`atoi`-hoz (sztring → szám), `<cstring>` — a
  `strcmp`-hoz (sztring-összehasonlítás argumentumokhoz).
- A projekt összes fejléce — mert a `main` az **összes** modult használja.
- `using namespace satlink;` — a `.cpp` legfelső szintjén kényelmes (00/7),
  hogy ne kelljen `satlink::` mindenhol.

### A jelkezelő és a belső eszközök (anonim névtér)

```cpp
namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_sigint(int) { g_stop = 1; }
```

- Anonim névtér (00/7): a belső eszközök nem szivárognak ki.
- `g_stop` — a leállító flag (00/19): `volatile sig_atomic_t`, mert jelkezelőből
  írjuk.
- `on_sigint(int)` — a kezelő. A paramétere (a jelszám) **névtelen**, mert nem
  használjuk; csak annyit tesz: `g_stop = 1`. A rövidség itt biztonsági követelmény.

```cpp
double ber_sweep(double t_s) {
    const double phase = std::fmod(t_s, 32.0);
    if (phase < 8.0)  return 0.0;
    if (phase < 16.0) return 1e-4;
    if (phase < 24.0) return 1e-3;
    return 5e-3;
}
```

- A **link-romlási menetrend**: egy 32 s-os ciklus, ami lépcsőzetesen romlik
  (tiszta → 1e-4 → 1e-3 → 5e-3), majd kezdi elölről. A `std::fmod` a lebegőpontos
  maradék (`<cmath>`): hol tartunk a 32 s-os cikluson belül. Ettől **önjáró** a
  demó: látod a CRC-hibák hullámzását.

### Az `Options` struct és a használat

```cpp
struct Options {
    double   ber       = -1.0;
    uint32_t seed      = 1;
    int      rate      = 8;
    double   duration  = 0.0;
    bool     once      = false;
    bool     color     = true;
    bool     garbage   = true;
};
```

- A parancssori beállítások egy helyen, **alapértékekkel** (00/5). A `ber = -1.0`
  egy „őrszem" érték: a negatív azt jelenti, „használd az automatikus sweepet".
- `void print_usage(const char* prog)` — kiírja a használati súgót (sima
  `printf`-ek).

### Argumentum-feldolgozás

```cpp
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
        ...
        else { std::fprintf(stderr, "unknown option: %s\n", a); print_usage(argv[0]); exit_code = 2; return false; }
    }
    return true;
}
```

- `int argc, char** argv` — a klasszikus C main-argumentumok: `argc` a darabszám,
  `argv` a sztringek tömbje (`char**` = pointerek tömbjére mutató pointer). `argv[0]`
  a program neve, `argv[1..]` a tényleges argumentumok.
- `Options& opt` — **referencia**: a függvény módosítja a hívó `opt`-ját (kimenet
  referencián át). `int& exit_code` ugyanígy a kilépési kódra.
- `need_value` egy **belső lambda** (00/12), `[&]` capture-rel (látja az `i`-t, az
  `a`-t, az `exit_code`-ot). Ellenőrzi, hogy van-e érték a kapcsoló után, és ha
  igen, `atof`-fal beolvassa, közben `++i`-vel **lépteti** a külső ciklusindexet
  (a referenciát látja). Tiszta példa: a lambda a környező ciklus állapotát
  manipulálja.
- `std::strcmp(a, "--ber")` — sztring-összehasonlítás; **0**-t ad, ha egyeznek,
  ezért `!std::strcmp(...)` = „egyezik". (Ez C-örökség; tömör.)
- A `--seed`-nél `double v`-be olvasunk, majd `static_cast<uint32_t>` — mert az
  `atof` `double`-t ad, de a mag egész.
- A `return false` (a `parse_args`-ból) azt jelzi a `main`-nek: „azonnal lépj ki"
  (hiba vagy `--help`).

### A `main` — felépítés

```cpp
int main(int argc, char** argv) {
    Options opt;
    int exit_code = 0;
    if (!parse_args(argc, argv, opt, exit_code)) return exit_code;

    if (!isatty(STDOUT_FILENO)) opt.color = false;
    if (opt.once && opt.ber < 0.0) opt.ber = 1e-3;

    std::signal(SIGINT, on_sigint);
```

- Beolvassuk az opciókat; ha `parse_args` `false`-t ad, kilépünk a megfelelő
  kóddal.
- `if (!isatty(...)) opt.color = false;` — ha nem terminálba írunk, **nincs szín**
  (00/20).
- `if (opt.once && opt.ber < 0.0) opt.ber = 1e-3;` — pillanatkép módban legyen
  látható CRC-hiba: ha a felhasználó nem adott BER-t, romlott linket választunk.
- `std::signal(SIGINT, on_sigint);` — bekötjük a Ctrl-C kezelőt (00/19).

```cpp
    Satellite    satellite(opt.seed);
    Framer       framer;
    NoisyChannel channel(opt.ber < 0.0 ? 0.0 : opt.ber, opt.seed ^ 0xA5A5u);
    Decoder      decoder;
    Dashboard    dashboard(opt.color);
    std::mt19937 noise_rng(opt.seed ^ 0x1234u);
```

- Létrehozzuk a cső **összes szereplőjét**. Mind **verem-objektum** (nincs
  `new`): a `main` blokkjának végén maguktól megsemmisülnek (RAII).
- `opt.seed ^ 0xA5A5u` / `^ 0x1234u` — a különböző véletlenforrásokat **eltérő**
  maggal indítjuk (XOR egy konstanssal), hogy ne korreláljanak, de mégis
  determinisztikusak legyenek.

```cpp
    const uint64_t snapshot_frames = static_cast<uint64_t>(opt.rate) * 8;
    Telemetry last_decoded{};
    bool       have_decoded = false;

    if (!opt.once) dashboard.begin();

    uint64_t frame_idx = 0;
    uint8_t  frame[kFrameSize];
```

- `snapshot_frames` — pillanatkép módban ennyi keretet futtatunk (8 „másodpercnyi").
- `last_decoded{}`, `have_decoded` — a legutóbb dekódolt telemetria és hogy van-e
  már.
- `if (!opt.once) dashboard.begin();` — élő módban előkészítjük a terminált.
- `uint8_t frame[kFrameSize];` — a **veremtömb** egy kerethez. `kFrameSize`
  fordítási idejű konstans (01-es fájl), ezért lehet vele tömböt méretezni — ez a
  `constexpr` haszna (00/3). Nincs heap.

### A fő ciklus

```cpp
    while (!g_stop) {
        const uint32_t t_ms = static_cast<uint32_t>(frame_idx * 1000 / opt.rate);
        const double   t_s  = t_ms / 1000.0;

        const double ber = (opt.ber < 0.0) ? ber_sweep(t_s) : opt.ber;
        channel.set_bit_error_rate(ber);
```

- `while (!g_stop)` — fut, amíg a Ctrl-C be nem állítja a flaget (00/19).
- `t_ms = frame_idx * 1000 / opt.rate` — a **küldetési idő** a keretindexből,
  nem a valós órából. Így a telemetria **megismételhető** (nem függ az ütemezési
  ingadozástól). `static_cast<uint32_t>` a `timestamp_ms` mezőtípusra.
- `ber = (opt.ber < 0.0) ? ber_sweep(t_s) : opt.ber;` — ha az őrszem-érték negatív,
  a sweepet használjuk, különben a rögzített BER-t. Beállítjuk a csatornán.

```cpp
        const Telemetry tm = satellite.sample(t_ms);
        const size_t n = framer.encode(tm, frame, sizeof(frame));
```

- (1) A műhold ad egy mintát, (2) a keretező bekeretezi a `frame` tömbbe.
  `sizeof(frame)` a tömb bájtmérete (= `kFrameSize`) — ezt adjuk kapacitásként.

```cpp
        if (opt.garbage && (noise_rng() % 100) < 12) {
            const int g = 1 + static_cast<int>(noise_rng() % 3);
            for (int k = 0; k < g; ++k) {
                decoder.feed(static_cast<uint8_t>(noise_rng() & 0xFF));
            }
        }
```

- **Vonali zaj** injektálás: 12% eséllyel 1–3 véletlen bájtot tolunk a dekóderbe
  a keret *előtt*, hogy a SYNC-keresés és resync élesben is dolgozzon. A
  `noise_rng() % 100 < 12` adja a ~12%-ot, a `% 3` az 1–3 darabot, a `& 0xFF` egy
  bájtot (00/14, 00/16).

```cpp
        channel.transmit(frame, n);
        for (size_t k = 0; k < n; ++k) {
            if (decoder.feed(frame[k])) {
                last_decoded = decoder.last_packet();
                have_decoded = true;
            }
        }
```

- (3) A csatorna **elrontja** a keretet (bithibák), majd (4) a bájtokat
  **egyesével** a dekóderbe adjuk — pont mint egy UART. Ha a `feed` `true`-t ad
  (érvényes keret), kiolvassuk a `last_packet()`-et.

```cpp
        if (!opt.once) {
            dashboard.render(last_decoded, have_decoded, decoder.stats(), ber,
                             channel.flipped_bits(), channel.total_bits(), t_s);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / opt.rate));
        }

        ++frame_idx;
        if (opt.once && frame_idx >= snapshot_frames) break;
        if (!opt.once && opt.duration > 0.0 && t_s >= opt.duration) break;
    }
```

- Élő módban (5) **kirajzoljuk** a dashboardot, majd **alszunk** egy keretnyit
  (00/15) a megadott képkockasebességgel.
- `++frame_idx;` — következő keret.
- Két kilépési feltétel: pillanatkép módban a keretszám elérése; élő módban a
  `--duration` lejárta.

```cpp
    if (opt.once) {
        const double t_s = static_cast<double>(frame_idx) / opt.rate;
        dashboard.render(last_decoded, have_decoded, decoder.stats(), opt.ber,
                         channel.flipped_bits(), channel.total_bits(), t_s);
    } else {
        dashboard.end();
    }
    return 0;
}
```

- Pillanatkép módban a ciklus **nem** rajzolt; itt a végén **egyszer** rajzolunk
  egy állóképet.
- Élő módban a `dashboard.end()` **visszaadja a kurzort** (00/19 takarítás).
- `return 0;` — siker.

---

## 2. `tests/test_main.cpp`

### A mini assert-keret

```cpp
int g_checks = 0;
int g_fails  = 0;
bool g_color = false;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_fails;                                                     \
            std::printf("    %s[FAIL]%s %s:%d  %s\n", red(), reset(),      \
                        __FILE__, __LINE__, #cond);                        \
        }                                                                  \
    } while (0)
```

- `CHECK` egy **makró** (00/18): számolja az ellenőrzéseket, és ha a feltétel
  hamis, kiírja a fájlt/sort (`__FILE__`, `__LINE__`) és a feltétel **szövegét**
  (`#cond`). A `do { } while(0)` az egy-utasítás-idióma.
- Globális számlálók (`g_checks`, `g_fails`) — a teljes futás összegzéséhez.

```cpp
void section(const char* name) {
    std::printf("%s%s>> %s%s\n", bold(), cyan(), name, reset());
}
```

- Egy szakaszcímet ír ki. A `red()`, `green()`, `cyan()` ... apró függvények,
  amik a színkódot adják, ha `g_color`, különben üres sztringet — így a teszt
  csőbe írva is olvasható marad (mint a dashboardnál).

### Segédek

```cpp
Telemetry make_sample() { ... t.temp_obc_c10 = -120; ... }
bool same(const Telemetry& a, const Telemetry& b) { return a.timestamp_ms == b.timestamp_ms && ...; }
std::vector<uint8_t> build_frame(Framer& f, const Telemetry& t) {
    uint8_t buf[kFrameSize];
    const size_t n = f.encode(t, buf, sizeof(buf));
    return std::vector<uint8_t>(buf, buf + n);
}
bool feed_all(Decoder& d, const std::vector<uint8_t>& bytes) {
    bool got = false;
    for (uint8_t b : bytes) if (d.feed(b)) got = true;
    return got;
}
```

- `make_sample()` — egy teszt-telemetria, **szándékosan negatív** mezőkkel
  (`temp_obc_c10 = -120`, `attitude_cdeg = -4500`), hogy az előjeles utat is
  teszteljük.
- `same(a, b)` — mezőnkénti egyenlőség-ellenőrzés (a `Telemetry`-nek nincs
  beépített `==`-ja, ezért kézzel).
- `build_frame(...)` — egy keretet épít a `Framer`-rel egy **verem**tömbbe, majd
  `std::vector`-ként adja vissza (a `vector(buf, buf+n)` tartomány-konstruktor,
  00/13). A teszt oldalon a kényelem fontosabb, mint a heap-mentesség.
- `feed_all(...)` — egy egész bájtsorozatot a dekóderbe ad; `true`, ha bármelyik
  bájt érvényes keretet zárt. Range-based for (00/11).

### Egy teszt példa (CRC)

```cpp
void test_crc() {
    section("CRC-16/CCITT-FALSE");
    const char* s = "123456789";
    CHECK(crc16_ccitt(reinterpret_cast<const uint8_t*>(s), 9) == 0x29B1);
    CHECK(crc16_ccitt(reinterpret_cast<const uint8_t*>(s), 0) == 0xFFFF);
}
```

- `reinterpret_cast<const uint8_t*>(s)` — a `const char*` sztringet
  **bájtpointerként** nézzük, mert a CRC azt vár (00/10).
- Az **ismert ellenőrzőérték**: `"123456789" → 0x29B1`. Ez bizonyítja, hogy a
  CRC szabványos (bármely online kalkulátorral egyezik). A `len = 0` eset a
  kezdőértéket (`0xFFFF`) adja.

A többi teszt (`test_serialization`, `test_framer`, `test_decode_roundtrip`,
`test_sync_hunt`, `test_two_frames`, `test_crc_catches_payload_flip`,
`test_crc_catches_crc_flip`, `test_resync_after_truncation`,
`test_bad_header_rejected`, `test_sequence_gap`, `test_torture`) ugyanezt a
mintát követi: állíts elő egy helyzetet, `feed`-eld a dekódert, `CHECK`-eld a
`stats()`-ot és a `last_packet()`-et. Egy-két kiemelés:

- **`test_decode_roundtrip`** ellenőrzi, hogy **csak az utolsó** bájt ad `true`-t
  (`completed_early` flaggel) — vagyis a `feed` szerződése pontos.
- **`test_torture`** egy `for (int i = 0; i < 1'000'000; ++i)` ciklusban (00/21
  számjegy-elválasztó!) random bájtokat tol be, majd ellenőrzi, hogy a dekóder
  **utána is** dekódol egy valódi keretet (nem ékelődött be).

### A futtató `main`

```cpp
int main() {
    g_color = isatty(STDOUT_FILENO);
    std::printf(...);
    test_crc();
    test_serialization();
    ...
    test_torture();

    if (g_fails == 0) { std::printf("ALL %d CHECKS PASSED", g_checks); return 0; }
    std::printf("%d/%d CHECKS FAILED", g_fails, g_checks);
    return 1;
}
```

- Sorban lefuttat minden tesztet, majd **összegez**. A **kilépési kód** fontos:
  `0` ha minden átment, `1` ha bukott — így a `make test` és egy esetleges CI is
  tudja, sikeres volt-e. (A `g_color = isatty(...)` ugyanaz a „terminál-e"
  trükk, 00/20.)

---

## 3. `Makefile`

```make
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -MMD -MP
```

- `:=` — **azonnali** értékadás (a make-ben; nem a halasztott `=`).
- `-std=c++17` — a nyelvi szabvány. `-Wall -Wextra -Wpedantic` — **minden
  figyelmeztetés** bekapcsolva (a tiszta fordítás célja). `-O2` — optimalizálás.
  `-Iinclude` — az `include/` mappában keresse a fejléceket.
- `-MMD -MP` — a fordító **fejléc-függőségi** fájlokat (`.d`) generál, így ha egy
  `.hpp` változik, a megfelelő `.cpp`-k újrafordulnak. (Profi build-higiénia.)

```make
LIB_SRC  := src/serialization.cpp src/crc16.cpp ... src/dashboard.cpp
LIB_OBJ  := $(LIB_SRC:.cpp=.o)
APP_OBJ  := src/main.o
TEST_OBJ := tests/test_main.o
BIN := satlink
TEST_BIN := satlink_tests
```

- `LIB_SRC` — a **közös** forrásmodulok (minden, kivéve a két belépési pontot).
- `$(LIB_SRC:.cpp=.o)` — minta-helyettesítés: minden `.cpp`-ből `.o` nevet csinál.
- A `main.o` és a `test_main.o` külön, mert mindkettő külön bináris belépési pontja.

```make
$(BIN): $(LIB_OBJ) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_BIN): $(LIB_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
```

- A `satlink` a közös modulokból + `main.o`-ból **linkel**; a `satlink_tests` a
  közös modulokból + `test_main.o`-ból. Így a **könyvtár-kód egyszer** fordul, és
  mindkét bináris használja.
- `$^` = az összes előfeltétel (a `.o`-k), `$@` = a cél (a bináris), `$<` = az
  első előfeltétel (a `.cpp`). Ezek a make „automatikus változói".
- A `%.o: %.cpp` egy **minta-szabály**: bármely `.cpp`-t `.o`-vá fordít (`-c` =
  „csak fordíts, ne linkelj").

```make
.PHONY: all run test snapshot clean
run: $(BIN) ; ./$(BIN)
test: $(TEST_BIN) ; ./$(TEST_BIN)
clean: ; rm -f ...
-include $(LIB_OBJ:.o=.d) ...
```

- `.PHONY` — ezek a célok **nem fájlok** (nincs `run` nevű fájl), hanem
  parancsok; a make-nek szólunk, hogy mindig fusson.
- `-include ... .d` — behúzza a generált függőségi fájlokat (a `-` elnyeli a hibát,
  ha még nincsenek).

---

## Záró kép

Most már **soronként** átlátod az egészet: a protokoll-konstansoktól (01) az
adóoldalon (02) át a dekóder állapotgépig (03), a szimulátoron és a látványos
kijelzőn (04) keresztül a fő ciklusig, a tesztekig és a buildig (05). A nyelvi
döntéseket a [00-s alapozó](00_nyelvi_alapozo.md) gyűjti egybe.

Ha egy konkrét sor vagy döntés még mindig homályos, az a leggyorsabb, ha
megnyitod a forrást és ezt az elemzést egymás mellett — vagy szólsz, és azt a
részt élőben, példákkal körbejárjuk.
