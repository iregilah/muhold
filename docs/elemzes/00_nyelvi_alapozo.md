# 00 — Nyelvi alapozó (C++11 és újabb)

Ez a fájl egy **szótár**: összeszedi azokat a nyelvi elemeket, amikkel a kódban
találkozol, és amik nagyrészt **C++11 után** kerültek a nyelvbe (téged onnan
veszítettek el). A többi elemzőfájl ezekre hivatkozik vissza. Amit már tudsz
(osztályok, öröklés, pointerek, szálak), azt itt nem ismétlem — a hangsúly a
modern szintaxison és a konkrét tervezési döntéseken van.

A sorrend nagyjából a „mennyire új / mennyire fontos" szerint halad.

---

## 1. A fordítás három lépése és a header/forrás szétválasztás

Egy `.cpp` fájl önálló **fordítási egység** (translation unit). A `g++` három
fázison viszi át:

1. **Preprocesszor** — a `#include` szövegszerűen bemásolja a fejlécet, a
   `#define`/`#pragma` direktívákat feldolgozza. A kimenet egy nagy szövegfolyam.
2. **Fordító** — ezt a szöveget tárgykóddá (`.o`) alakítja. Itt derülnek ki a
   szintaktikai és típushibák. Minden `.cpp` **külön** fordul.
3. **Linker** — a `.o` fájlokat és a könyvtárakat egyetlen futtatható programmá
   fűzi össze, feloldva a kereszthivatkozásokat (egyik `.cpp` hívja a másik
   függvényét).

Ezért van minden modulnak **`.hpp`** (deklaráció: „mi létezik", a fordító ennyit
lát, amikor egy másik fájl `#include`-olja) és **`.cpp`** (definíció: „hogyan
működik"). A `framer.cpp` `#include "framer.hpp"`-t tesz, hogy lássa a saját
deklarációit, és `#include "crc16.hpp"`-t, hogy hívhassa a CRC-t — de a CRC
*kódját* nem látja, csak a **prototípusát**; a tényleges összekötést a linker
végzi. Ez a klasszikus C/C++ modell, beágyazott projektekben is ez a norma.

### `#pragma once`

Minden fejléc első sora `#pragma once`. Ez azt mondja: „ezt a fájlt fordítási
egységenként csak egyszer másold be". A klasszikus alternatíva a háromsoros
include-guard (`#ifndef X / #define X / ... / #endif`). A `#pragma once` nem
szabványos, de minden valódi fordító ismeri, rövidebb és nem lehet elrontani a
névütközéssel. Miért kell egyáltalán? Mert ha `A.hpp` és `B.hpp` is behúzza
`protocol.hpp`-t, és egy `.cpp` mindkettőt, akkor a `protocol.hpp` kétszer
kerülne be → kétszeres definíció → fordítási hiba.

---

## 2. Fix szélességű egészek — `<cstdint>`

```cpp
#include <cstdint>
uint8_t, uint16_t, uint32_t, uint64_t   // előjel nélküli, 8/16/32/64 bit
int16_t                                  // előjeles, 16 bit
```

A sima `int`, `short`, `long` mérete **platformfüggő** (egy `int` lehet 16, 32
vagy 64 bites). Egy protokollban, ahol pontosan tudni kell, hány bájt megy a
vezetéken, ez elfogadhatatlan. A `<cstdint>` típusai **garantáltan** pontosan
annyi bitesek, amennyit a nevük mond. Beágyazott és kommunikációs kódban szinte
mindig ezeket használjuk a sima `int` helyett.

A `<cstddef>`-ből jön a **`size_t`**: egy előjel nélküli egész, ami elég nagy
bármilyen objektum méretének/indexének tárolására. Tömbméretekhez, indexekhez,
`sizeof` eredményéhez ezt használjuk. (Figyelem: előjel nélküli, ezért a
`size_t i` és egy `int` összehasonlítása fordítói figyelmeztetést adhat — erre
később lesz példa.)

A `c` előtag (`cstdint` a C `stdint.h` helyett): a C szabványkönyvtár fejléceit
C++-ban `c`-vel és `.h` nélkül illik behúzni; így a nevek a `std::` névtérbe is
bekerülnek.

---

## 3. `const`, `constexpr`, `#define`

Háromféleképpen lehet „konstanst" csinálni; mi tudatosan választunk:

```cpp
#define SYNC 0x1ACFFC1D          // RÉGI, C-stílus — kerüljük
const uint32_t kSyncWord = ...;  // futásidejű konstans, van típusa
constexpr uint32_t kSyncWord = 0x1ACFFC1D;  // EZT használjuk
```

- A **`#define`** csak szövegcsere a preprocesszorban: nincs típusa, nincs
  névtere, nem lehet debuggerből látni. Hibaforrás.
- A **`const`** egy rendes, típusos változó, amit nem írhatsz felül.
- A **`constexpr`** (C++11) annyiban több a `const`-nál, hogy garantáltan
  **fordítási időben** kiszámolható, tehát használható ott is, ahol a nyelv
  fordítási idejű állandót követel (tömbméret, `case` címke, sablonparaméter).

A `constexpr` a modern C++ válasza a `#define`-ra konstansok esetén: minden előny
(fordítási idejű érték), semmi hátrány (van típusa, névtere, hibakereshető).
A kódban a `k` előtag (`kSyncWord`, `kFrameSize`) egy elterjedt elnevezési
konvenció a fordítási idejű konstansokra.

`constexpr` **függvény** is van: olyan függvény, amit a fordító ki tud értékelni
fordítási időben, ha az argumentumai is állandók. Ebben a projektben a konstans
*értékek* `constexpr`-ek, a függvények nem — de jó tudni, hogy létezik.

---

## 4. `enum class` — hatókörös felsorolás

```cpp
enum class Mode : uint8_t { Safe = 0, Nominal = 1, Payload = 2, Comms = 3 };
```

A **régi** `enum` két bajjal járt: (1) a nevei „kifolytak" a környező hatókörbe
(`Safe` simán látszott mindenhol, névütközést okozva), és (2) automatikusan
egésszé konvertálódott (`int x = Safe;` simán átment, pedig értelmetlen).

A **`enum class`** (C++11, „scoped enum"):
- A neveket a típushoz köti: `Mode::Safe`-et kell írni, nem `Safe`-et. Nincs
  névszennyezés.
- **Nem** konvertálódik magától egésszé: `int x = Mode::Safe;` fordítási hiba.
  Ha tényleg számot akarsz belőle, ki kell mondani: `static_cast<uint8_t>(m)`.
  Ez véd a véletlen összekeveréstől.
- A `: uint8_t` megadja az **alaptípust**: a `Mode` pontosan 1 bájton tárolódik.
  Ez fontos, mert a protokollban a mode 1 bájt — így a `struct`-ban is annyi.

A `switch (m) { case Mode::Safe: ... }` továbbra is működik, sőt: ha lefedsz
minden `enum class` értéket, a fordító nem panaszkodik hiányzó esetre.

---

## 5. `struct` vs `class`, tagok, alapértelmezett tagérték

C++-ban a `struct` és a `class` **majdnem ugyanaz**: az egyetlen különbség az
alapértelmezett láthatóság (`struct`-ban minden `public`, `class`-ban `private`).
Konvenció: `struct`-ot „buta adatcsomagra" használunk (csak mezők, mint a
`Telemetry`), `class`-t olyan típusra, aminek viselkedése/invariánsa van és
elrejti a belsőit (mint a `Decoder`).

**Alapértelmezett tagérték** (C++11) — ez új neked:

```cpp
struct Telemetry {
    uint32_t timestamp_ms = 0;   // ha senki nem adja meg, 0 lesz
    Mode     mode = Mode::Safe;
};
```

Régen a tagokat a konstruktor inicializáló listájában kellett beállítani. C++11
óta közvetlenül a tag mellé írhatod az alapértéket. Így egy `Telemetry t{};`
eleve nullázott, definiált állapotú — nincs „szemét" kezdőérték. A `Decoder`
osztály minden tagja is így kap biztonságos kezdőértéket (`idx_ = 0`,
`state_ = RxState::HuntSync`, stb.), ezért nem kell külön konstruktort írni.

---

## 6. Inicializálás kapcsos zárójellel — `{}` (uniform initialization)

C++11 bevezette az egységes `{}`-es inicializálást. Több helyen látod:

```cpp
Telemetry t{};                 // minden mező az alapértékére (itt: nullák)
DecoderStats stats_{};         // ugyanez, tagként
Telemetry last_{};
std::vector<uint8_t>{0x00, 0xFF, 0x1A};  // lista-inicializálás (a tartalom)
return {label, col, sev};      // egy struct összeállítása a mezők sorrendjében
```

Két dolgot érdemes tudni:
- **`T x{};`** „value-initialization": a beépített típusokat **nullázza**.
  A `T x;` (zárójel nélkül) ezzel szemben lokális változónál **definiálatlan
  szemetet** hagyhat. Ezért írunk `{}`-t, ha tiszta kezdőállapot kell.
- **Aggregátum-inicializálás:** a `Telemetry`/`LinkInfo`-féle egyszerű
  struct-okat fel lehet tölteni a mezők **felsorolási sorrendjében**:
  `return {label, col, sev};` pontosan a `LinkInfo{ .label=, .color=, .severity= }`
  rövidítése. Vigyázni kell a sorrendre — a fordító a deklaráció sorrendjét
  követi.

A tesztekben látott `Telemetry t; t.timestamp_ms = 123456; ...` a másik,
„kézzel mezőnként" stílus — funkcionálisan ugyanaz, csak olvashatóbb sok mezőnél.

---

## 7. Névterek — `namespace`, anonim névtér, `using namespace`

```cpp
namespace satlink { ... }       // minden projektkód ebben él
```

A **névtér** megakadályozza a névütközést: a mi `Decoder`-ünk valójában
`satlink::Decoder`, így nem ütközik egy könyvtár `Decoder`-ével. A `.cpp`-k a
kódjukat ugyanabba a `namespace satlink { ... }`-ba teszik, mint amibe a fejléc
deklarálta — így a definíció és a deklaráció összetartozik.

**Anonim (név nélküli) névtér** — ezt a `dashboard.cpp`-ben és máshol látod:

```cpp
namespace {            // nincs neve
    constexpr int kInner = 62;
    std::string fmt(...) { ... }
}
```

Ami egy névtelen névtérben van, az **csak az adott `.cpp`-ben látszik** (belső
linkelés). Ez a modern megfelelője a régi fájlszintű `static`-nak: a `fmt`,
`make_bar`, `Row` segédeszközök így nem szivárognak ki más fordítási egységekbe,
nem okoznak linker-ütközést. Privát „belső konyha" a fájlnak.

**`using namespace satlink;`** (a `main.cpp`-ben és a tesztekben): ezzel a `satlink::`
előtagot elhagyhatod azon a fájlon belül. Fejlécben **soha** ne tedd (mindenkire
ráerőltetné), de egy `.cpp` legfelső szintjén kényelmes.

---

## 8. Konstruktor inicializáló lista és `const` tagfüggvény

```cpp
NoisyChannel::NoisyChannel(double bit_error_rate, uint32_t seed)
    : ber_(bit_error_rate), rng_(seed) {}      // <- inicializáló lista
```

A `:` utáni rész a **taginiciáló lista**: a tagokat **közvetlenül** építi fel a
megadott értékkel, még a konstruktor törzse előtt. Ez nem új (C++98 óta van), de
fontos: a `rng_(seed)` itt a `std::mt19937` motort a `seed` magértékkel hozza
létre. Ha a törzsben (`{ }`-en belül) adnánk értéket, az már egy
felül*írás* lenne, nem építés — referenciáknál és `const` tagoknál ez nem is
működne, ezért a lista a helyes minta.

```cpp
const DecoderStats& stats() const { return stats_; }
```

A függvény neve utáni **`const`** azt ígéri: ez a tagfüggvény **nem módosítja**
az objektumot. Így egy `const Decoder&`-en is hívható, és a fordító betartatja,
hogy tényleg ne írj bele. A `getter`-ek (lekérdezők) mindig `const`-ok.

---

## 9. Referenciák, `const&`, és mikor pointer

Te ismered a pointereket; a referenciát itt főleg **másolás elkerülésére**
használjuk:

```cpp
size_t serialize_telemetry(const Telemetry& t, uint8_t* out);
```

- **`const Telemetry& t`** — a `Telemetry`-t **nem másoljuk le** (a struct 13+
  bájt), csak hivatkozunk rá, és a `const` garantálja, hogy nem módosítjuk. Ez a
  „bemenetnek átadom, de nem nyúlok hozzá" alapminta.
- **`uint8_t* out`** — itt **pointert** használunk, mert (a) egy nyers
  bájtpufferre mutat, amibe **írunk**, és (b) ez a C-s, beágyazott interfész:
  „adj egy puffert, beleírom". A pointer + hossz páros (`out`, `out_cap`) a
  klasszikus „nincs heap, a hívó adja a memóriát" minta.

Ökölszabály a projektben: **referencia**, ha egy meglévő objektumra hivatkozunk
és nem akarunk null-t megengedni; **pointer**, ha nyers memóriablokkról vagy
opcionális/írható tárról van szó.

---

## 10. Nevesített típuskonverziók — `static_cast`, `reinterpret_cast`

A régi C-cast (`(uint16_t)x`) mindent megenged, és elrejti, mi történik. A C++
**nevesített** castjai kimondják a szándékot, és a fordító ellenőrzi:

```cpp
static_cast<uint16_t>(value)   // értelmes, biztonságos konverzió számok közt
static_cast<uint8_t>(t.mode)   // enum class -> szám (kötelező kimondani)
reinterpret_cast<const uint8_t*>("123456789")  // "ugyanazt a memóriát nézd más típusként"
```

- **`static_cast`** — „normál" konverzió, aminek a fordító ismeri az értelmét:
  `double`→`int`, `int`→`uint16_t`, `enum class`→egész. Ezt látod a leggyakrabban.
- **`reinterpret_cast`** — „ugyanazokat a bájtokat tekintsd más típusú
  pointernek". Itt egy `const char*`-ot (a sztring) `const uint8_t*`-ként
  nézünk, mert a CRC bájtokat vár. Veszélyesebb eszköz, ezért külön neve van,
  hogy könnyű legyen kiszúrni a kódban.

Miért nem a régi C-cast? Mert az néma: nem látod, hogy „ártalmatlan
számkonverzió" vagy „veszélyes újraértelmezés" történik-e. A nevesített cast
dokumentál és szűkít.

A `static_cast<uint16_t>(...)` sokszor csak azért kell, hogy **elnémítsuk a
szűkítési figyelmeztetést**: a bitműveletek eredménye gyakran `int` (lásd 12.
pont), és ha azt egy `uint16_t`-be tesszük, a fordító figyelmeztet — a
`static_cast` azt mondja: „tudom, szándékos".

---

## 11. `auto` és tartomány-alapú `for`

```cpp
auto bytes = build_frame(f, tx);        // a fordító kitalálja a típust
for (uint8_t b : bytes) { ... }         // range-based for: a bytes minden eleme
for (auto& b : bytes) { ... }           // ugyanaz, referenciával (nem másol)
```

- **`auto`** (C++11) — a fordító a jobb oldalból **kikövetkezteti** a típust.
  Nem dinamikus típus (ez nem Python!), csak rövidítés: `auto bytes` itt
  pontosan `std::vector<uint8_t>` lesz, fordítási időben rögzítve. Hosszú STL
  típusneveknél nagyon hasznos.
- **Range-based for** (C++11) — „minden elemre". A régi
  `for (size_t i = 0; i < v.size(); ++i)` helyett `for (auto& e : v)`. A `:`
  utáni rész a bejárandó tároló. Ha módosítani is akarsz vagy nem akarsz
  másolni, `auto&` (referencia); ha csak olvasol kis elemeket (`uint8_t`),
  érték szerint is mehet.

---

## 12. Lambdák — helyben definiált függvények

Ezt biztosan nem tanultad: a **lambda** egy névtelen függvény, amit egy változóba
tehetsz vagy azonnal hívhatsz. A `dashboard.cpp` és a `main.cpp` tele van vele.

```cpp
auto emit = [&](Row& r) { r.pad(kInner); out += "║"; out += r.buf; ... };
emit(r);   // úgy hívod, mint egy függvényt
```

Anatómia: `[capture](paraméterek) { törzs }`.

- A **`[...]`** a **capture-lista**: milyen környező változókat lát a lambda.
  - `[&]` — **mindent referencia szerint** lát (módosíthat is). A `dashboard`-
    ban ezért tudja az `emit` a külső `out` sztringhez fűzni a sorokat.
  - `[=]` — mindent **másolatként** lát.
  - `[]` — semmit (csak a paramétereit).
- A **paraméterek** és a **törzs** ugyanaz, mint egy függvénynél.

Miért jó? Mert a render-logikában sok apró, csak ott értelmes „mini-függvény"
van (`emit`, `rule`, `channel_row`, `temp_row`, `counter_row`). Lambdaként
helyben tartjuk őket, látják a közös állapotot (`out`, `color_`), és nem
szennyezik a névteret. A `main.cpp`-ben a `need_value` lambda az argumentum-
értékek beolvasását egységesíti.

(Technikailag a lambda egy fordító által generált, `operator()`-rel rendelkező
névtelen osztály — de ezt nem kell fejben tartani a használatához.)

---

## 13. STL típusok: `std::string`, `std::vector`

Ezeket talán használtad, de frissítsük. Mindkettő **sablon**-alapú, dinamikus
méretű tároló, ami **magától kezeli a memóriát** (RAII: a destruktora felszabadít
— neked nincs `new`/`delete` dolgod):

```cpp
std::string s;            // dinamikus karakterlánc
s += "║";                 // hozzáfűzés (a memória magától nő)
s.reserve(4096);          // előre lefoglal 4096 bájtot (kevesebb újrafoglalás)
s.size();                 // hossz BÁJTBAN (UTF-8-nál != látható oszlop!)
s.c_str();                // C-stílusú const char* (printf, fputs)

std::vector<uint8_t> v;   // dinamikus tömb
v.push_back(0x55);        // elem a végére
v.insert(v.end(), a.begin(), a.end());  // másik tartomány hozzáfűzése
v.begin(), v.end();       // iterátorok (a tartomány eleje/vége)
std::vector<uint8_t>(buf, buf + n);     // tartomány-konstruktor: [buf, buf+n)
```

Fontos csapda, ami később előkerül: a `std::string::size()` a tárolt **bájtok**
számát adja, **nem** a képernyőn látható oszlopokét. Egy UTF-8 `°` 2 bájt, de 1
oszlop. A dashboard ezért nem bízik a `size()`-ban a doboz igazításánál.

A `std::vector`-t a TX (adó) és a tesztek oldalán használjuk, ahol a kényelem
fontos. A **dekóderben szándékosan NINCS** vector — ott fix tömb van (lásd a 22.
pontot), mert az a beágyazott megkötés.

---

## 14. Véletlenszám: `<random>` (`std::mt19937`)

A régi `rand()` rossz minőségű és nehéz jól használni. C++11 hozott egy rendes
keretet:

```cpp
#include <random>
std::mt19937 rng(seed);                                  // a generátor (motor)
std::uniform_real_distribution<double> dist(0.0, 1.0);   // [0,1) egyenletes eloszlás
double x = dist(rng);                                     // egy minta
```

A modell: van egy **motor** (`mt19937` = Mersenne Twister, jó minőségű
álvéletlen) és egy **eloszlás**, ami a motor nyers bitjeit a kívánt
tartományba/alakba képezi. A motort egy **maggal** (`seed`) indítjuk — azonos
mag → azonos sorozat, ezért **megismételhetők** a futások (tesztelésnél
aranyat ér). A `noise_{-1.0, 1.0}` a műholdnál mérési zajt ad; a csatornában a
`dist_(rng_) < ber_` dönt, hogy egy bitet átbillentünk-e.

A `main.cpp`-ben látod a `noise_rng() % 100` formát is: a motort közvetlenül,
eloszlás nélkül hívjuk, ami egy nyers `uint32_t`-t ad — `% 100`-zal 0..99
közé szorítjuk. (Eloszlás nélkül enyhén torzít, de „vonali zajhoz" tökéletes.)

---

## 15. Idő és alvás: `<chrono>` és `<thread>`

```cpp
#include <chrono>
#include <thread>
std::this_thread::sleep_for(std::chrono::milliseconds(1000 / opt.rate));
```

A `<chrono>` **típusos időtartamokkal** dolgozik: `std::chrono::milliseconds(125)`
nem csak egy szám, hanem „125 ezredmásodperc" — a típus hordozza a mértékegységet,
így nem keversz össze ms-ot és s-et. A `std::this_thread::sleep_for(...)` az
**aktuális szálat** altatja a megadott ideig. Ezt használja a fő ciklus, hogy a
dashboard a megadott képkockasebességgel frissüljön. (Szálkezelést ismersz; itt
csak egyetlen szál van, és csak altatjuk.)

---

## 16. Bitműveletek, maszkok, integer promotion

A protokoll- és CRC-kód lelke. Az operátorok:

```cpp
x << 8     // balra told 8 bittel (× 256)
x >> 8     // jobbra told 8 bittel (÷ 256, előjel nélkülinél)
a | b      // bitenkénti VAGY (bitek összerakása)
a & 0xFF   // bitenkénti ÉS maszkkal (csak az alsó 8 bit marad)
a ^ b      // bitenkénti KIZÁRÓ VAGY (bitbillentés, CRC)
1u << bit  // a `bit`-edik bit beállítva (maszk)
```

Két fontos, könnyen elfelejtett részlet:

- **Integer promotion:** a `uint8_t`/`uint16_t` operandusok a műveletek előtt
  **`int`-té léptetődnek elő**. Ezért egy `uint16_t a, b; a = a << 1;` jobb
  oldala valójában `int` típusú, és ha visszateszed `uint16_t`-be, a fordító
  szűkítésre figyelmeztethet → innen a sok `static_cast<uint16_t>(...)`.
- **`1u`** a `1` helyett: az `u` utótag előjel nélkülivé teszi a literált.
  A `1 << 31` előjeles `int`-en **definiálatlan viselkedés** lehet; `1u << ...`
  biztonságos. Maszkoknál mindig az előjel nélküli forma a helyes.

A `0xFF`, `0x8000`, `0x1021` stb. **hexadecimális literálok** — bitmintáknál a
hexa olvashatóbb, mint a tízes számrendszer (egy hexa számjegy = 4 bit).

---

## 17. A `printf`-család és a változó argumentumszám (`varargs`)

```cpp
#include <cstdio>
std::printf("exit code: %d\n", code);
std::fputs(out.c_str(), stdout);    // nyers sztring kiírása, formázás nélkül
std::fflush(stdout);                // a pufferelt kimenet azonnali kiírása
```

- A **`printf`** formátumsztringgel dolgozik: `%d` egész, `%llu` `unsigned long
  long`, `%.2f` két tizedes lebegőpont, `%02d` kétjegyű, nullával töltött egész,
  `%.1e` tudományos alak, `%s` sztring, `%%` egy szó szerinti `%`.
- A **`fputs`** csak kiír egy kész sztringet (gyorsabb, ha már megformáztad).
- A **`fflush`** kikényszeríti a kiírást: a terminál pufferel, és a dashboardnál
  azt akarjuk, hogy a teljes képkocka **azonnal** megjelenjen.

A `dashboard.cpp` `fmt(...)` segédfüggvénye saját **varargs** (változó
argumentumszámú) függvény — ez tisztán C-örökség:

```cpp
std::string fmt(const char* f, ...) {     // a ... = "tetszőleges további argumentum"
    char buf[96];
    va_list ap; va_start(ap, f);          // az argumentumok bejárása
    std::vsnprintf(buf, sizeof(buf), f, ap);  // mint printf, de sztringbe és biztonságosan
    va_end(ap);
    return std::string(buf);
}
```

A `vsnprintf` a `printf` „pufferbe, korláttal" változata: legfeljebb
`sizeof(buf)` bájtot ír, így **nem lépi túl** a `buf`-ot (a régi `sprintf` igen —
klasszikus buffer overflow). A `...` és a `va_list`/`va_start`/`va_end` a C
varargs-mechanizmusa. Modern C++-ban inkább kerülnénk (nem típusbiztos), de egy
apró printf-burkolóhoz elfogadható és tömör.

---

## 18. Makrók: `do { } while (0)`, `#`, `__FILE__`, `__LINE__`

A tesztekben:

```cpp
#define CHECK(cond)                                            \
    do {                                                       \
        ++g_checks;                                            \
        if (!(cond)) {                                         \
            ++g_fails;                                         \
            std::printf("[FAIL] %s:%d  %s\n",                  \
                        __FILE__, __LINE__, #cond);            \
        }                                                      \
    } while (0)
```

- A `\` a sor végén: a makró **több sorra** nyúlik (a preprocesszor egy logikai
  sornak látja).
- **`do { ... } while (0)`** — klasszikus makró-idióma. Azért kell, hogy a makró
  egyetlen utasításként viselkedjen akkor is, ha `if (...) CHECK(x); else ...`
  környezetben használod — pontosvesszővel a végén, meglepetés nélkül.
- **`#cond`** — a `#` a makróban **szöveggé alakítja** az argumentumot (token
  stringizálás). Így a `CHECK(a == b)` hibaüzenete tartalmazza a szó szerinti
  `"a == b"` szöveget.
- **`__FILE__`, `__LINE__`** — beépített preprocesszor-makrók: az aktuális fájl
  neve és sorszáma. Így a bukott teszt megmondja, **hol** bukott. (`%s:%d` →
  „fájl:sor".)

Miért makró és nem függvény? Mert a `__LINE__`-nak és a `#cond`-nak a **hívás
helyén** kell kiértékelődnie — egy függvény mindig a saját sorát/`""`-ját látná.

---

## 19. `volatile`, `sig_atomic_t`, jelkezelés

A `main.cpp`-ben:

```cpp
volatile std::sig_atomic_t g_stop = 0;
void on_sigint(int) { g_stop = 1; }
std::signal(SIGINT, on_sigint);
```

A `SIGINT` (Ctrl-C) **aszinkron** érkezik: bármelyik pillanatban megszakíthatja a
programot, és lefuttatja az `on_sigint` kezelőt. Két finomság:

- **`std::sig_atomic_t`** — az egyetlen típus, amit egy jelkezelőből biztonságos
  írni; az írása „oszthatatlan" (nem szakad meg félúton).
- **`volatile`** — szól a fordítónak: ezt a változót **kívülről** is
  megváltoztathatják, ezért ne optimalizálja ki (mindig a memóriából olvassa
  újra). E nélkül a fordító azt hihetné, hogy a `g_stop` sosem változik a
  cikluson belül, és „örökre igaz" feltétellé alakíthatná.

A kezelő csak annyit tesz, hogy `g_stop = 1` — a tényleges takarítás (kurzor
visszaadása) a fő ciklusban történik, ami a következő körben látja a flaget és
kilép. Ez a helyes minta: a jelkezelő legyen a lehető legrövidebb.

---

## 20. POSIX a szabványon kívül: `<unistd.h>`, `isatty`

```cpp
#include <unistd.h>
if (!isatty(STDOUT_FILENO)) opt.color = false;
```

Az `isatty` **nem** C++ szabvány, hanem **POSIX** (Linux/Unix). Megmondja, hogy a
kimenet egy **valódi terminál**-e, vagy fájlba/csőbe irányítjuk. Ha nem terminál,
kikapcsoljuk a színeket és a kurzortrükköket — különben a `make snapshot >
fajl.txt` tele lenne értelmetlen escape-kódokkal. Ez a fajta „futásidőben
megnézem, hova írok" gyakori beágyazott/rendszerprogramozói fogás.

---

## 21. Apróságok, amikkel találkozol

- **Számjegy-elválasztó `'`** (C++14): `1'000'000` ugyanaz, mint `1000000`, csak
  olvashatóbb. A tesztek kínzótesztjében látod.
- **Háromtagú operátor `?:`** sűrűn: `a ? b : c`. Színválasztásnál láncolva is:
  `x >= 3600 ? zöld : x >= 3450 ? sárga : piros` (jobbról balra zárójeleződik).
- **`nullptr`** (C++11) a régi `NULL`/`0` helyett null-pointerre — a kódban
  közvetlenül nem nagyon van rá szükség, de ha látod, ez a modern null.
- **`++x` vs `x++`**: a kódban előtag-növelést (`++stats_.bytes_seen`) használunk,
  ahol az érték nem kell azonnal — apró, de bevett szokás (objektumoknál
  hatékonyabb lehet, és kifejezi: „csak növeld").

---

## 22. Amit tudatosan NEM használtam — és miért

Ez ugyanolyan fontos, mint amit használtam. Beágyazott/repülő szoftverben ezek a
döntések számítanak:

- **Nincs `memcpy` a struct → vezeték irányban.** Csábító lenne a `Telemetry`-t
  egyben a pufferbe másolni, de az (a) a fordító **kitöltő bájtjait** is
  átvinné, és (b) a **host bájtsorrendjét** (little-endian x86) tenné a
  vezetékre. Ezért **mezőnként, big-endian** szerializálunk (lásd a 02-es
  fájlt). Ez az egyik legfontosabb tanulság.
- **Nincs `__attribute__((packed))` a vezetékre.** A „pakold össze a struct-ot
  hézag nélkül" trükk létezik, de törékeny (igazítási gondok egyes chipeken) és
  nem oldja meg a bájtsorrendet. A kézi szerializálás hordozható és egyértelmű.
- **Nincs `new`/`delete`, nincs heap a dekóderben.** A `Decoder` minden puffere
  **fix méretű tag** (`uint8_t buf_[...]`). Repülő szoftverben a dinamikus
  foglalás kerülendő (töredezettség, kiszámíthatatlan időzítés, foglalási hiba).
  A `std::vector`-t csak a kényelmi (TX, teszt) oldalon engedjük meg.
- **Nincs kivétel (`throw`/`try`).** A hibákat **visszatérési értékkel** és
  **számlálókkal** kezeljük (`feed()` `bool`-t ad, a `DecoderStats` számol). Ez a
  beágyazott stílus: determinisztikus, nincs rejtett vezérlésátadás.
- **Nincs blokkolás.** A `feed()` egy bájtot dolgoz fel és **azonnal visszatér**.
  Sosem vár adatra. Ettől használható egy valódi UART-megszakításból.

---

Ennyi a szótár. A többi fájlban (`01`–`05`) ezekre hivatkozom vissza, és
soronként végigmegyünk a kódon. Ha egy konstrukció nem ugrik be, lapozz vissza
ide a megfelelő ponthoz.
