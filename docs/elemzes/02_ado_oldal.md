# 02 — Az adóoldal: framer és channel

A `Framer` az adó (műhold), ami a `Telemetry`-ből teljes keretet rak. A
`NoisyChannel` a rádiólink, ami elrontja a kereteket. Mindkettő **osztály**
(00/5), mert van belső állapotuk (futó sorszám, illetve a véletlenmotor és a
számlálók).

---

## 1. `include/framer.hpp`

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "protocol.hpp"

namespace satlink {

class Framer {
public:
    size_t encode(const Telemetry& t, uint8_t* out, size_t out_cap);
    uint16_t next_seq() const { return seq_; }

private:
    uint16_t seq_ = 0;
};

} // namespace satlink
```

- `class Framer` — osztály, mert **állapotot** tart (a `seq_` sorszámot a
  hívások között). Egy `struct` is működne, de a `class` jelzi: „ennek
  viselkedése van, a belsője privát".
- `public:` / `private:` — láthatósági szakaszok. Kívülről csak az `encode` és a
  `next_seq` érhető el; a `seq_` rejtett.
- `size_t encode(const Telemetry& t, uint8_t* out, size_t out_cap);`
  - `const Telemetry& t` — a bemenet referenciaként, nem másolva, nem módosítva
    (00/9).
  - `uint8_t* out` — a **hívó által adott** kimeneti puffer (nincs heap — 00/22).
  - `size_t out_cap` — a puffer **kapacitása**. Ezzel tudjuk ellenőrizni, hogy
    belefér-e a keret, mielőtt írunk → nincs túlírás.
  - visszatérés `size_t`: hány bájtot írtunk.
- `uint16_t next_seq() const { return seq_; }` — egy **inline getter** (a teste a
  fejlécben van). A `const` (00/8) ígéri: nem módosít. A tesztek ezzel
  ellenőrzik, hogy nő-e a sorszám.
- `uint16_t seq_ = 0;` — a privát állapot, **alapértékkel** (00/5). A záró `_` a
  privát tagok elnevezési konvenciója a projektben.

---

## 2. `src/framer.cpp`

```cpp
#include "framer.hpp"
#include "crc16.hpp"
#include "serialization.hpp"

namespace satlink {

size_t Framer::encode(const Telemetry& t, uint8_t* out, size_t out_cap) {
    if (out_cap < kFrameSize) {
        return 0;
    }
```

- A három `#include`: a saját fejléc + a két eszköz, amit használunk (CRC,
  szerializáció). A `Framer` `.cpp`-je **látja** ezek prototípusát; az
  összekötést a linker végzi (00/1).
- `size_t Framer::encode(...)` — a `Framer::` előtag mondja meg, hogy ez a fejlécben
  deklarált `encode` **definíciója** (nem egy szabad függvény).
- `if (out_cap < kFrameSize) return 0;` — **védekezés**: ha a hívó puffere
  kisebb, mint egy keret (23 bájt), nem írunk semmit, 0-t adunk vissza. Sosem
  írunk a puffer fölé (00/9, 00/22).

```cpp
    size_t i = 0;

    put_u32(&out[i], kSyncWord);
    i += kSyncSize;
```

- `size_t i = 0;` — egy **íráspozíció** (kurzor) a pufferben. Ahogy haladunk,
  léptetjük. Ez tisztább, mint mindenhova fix indexet írni.
- `put_u32(&out[i], kSyncWord)` — a 4 bájtos SYNC-et big-endian kiírjuk a `i=0`
  pozícióra (`&out[0]`). A SYNC **nincs** a CRC-ben.
- `i += kSyncSize;` — a kurzor 4-gyel előre (most `i == 4`).

```cpp
    const size_t crc_region_start = i;
    out[i++] = kVersionType;
    put_u16(&out[i], seq_);
    i += 2;
    out[i++] = static_cast<uint8_t>(kPayloadSize);
```

- `const size_t crc_region_start = i;` — **megjegyezzük**, hol kezdődik a
  CRC-vel fedett rész (most `i == 4`). A CRC innen számolódik, nem a SYNC-től —
  ezért kell elmenteni. `const`, mert nem változik többé.
- `out[i++] = kVersionType;` — beírjuk a verziót (`0x01`), és a `i++` a beírás
  **után** lépteti a kurzort (utótag-növelés: előbb használ `i`-t indexként,
  aztán növel). Most `i == 5`.
- `put_u16(&out[i], seq_); i += 2;` — a 2 bájtos sorszám big-endian (`i=5`),
  majd kurzor +2 → `i == 7`.
- `out[i++] = static_cast<uint8_t>(kPayloadSize);` — a payload-hossz (13). A
  `kPayloadSize` `size_t`, ezért `uint8_t`-re castoljuk (a fejlécmező 1 bájt). Most
  `i == 8`.

```cpp
    serialize_telemetry(t, &out[i]);
    i += kPayloadSize;
```

- A 13 bájtos payloadot a `serialize_telemetry` írja a `&out[8]`-ra (01-es
  fájl). Kurzor +13 → `i == 21`.

```cpp
    const uint16_t crc = crc16_ccitt(&out[crc_region_start], kCrcCoveredSize);
    put_u16(&out[i], crc);
    i += kCrcSize;
```

- `crc16_ccitt(&out[crc_region_start], kCrcCoveredSize)` — a CRC-t a
  **`crc_region_start`-tól** (4. bájt) számoljuk, **`kCrcCoveredSize` (17)**
  hosszon = fejléc + payload. Ez a kritikus rész: pontosan a 4..20 bájtok.
- `put_u16(&out[i], crc); i += 2;` — a 2 bájtos CRC big-endian a végére
  (`i=21`), kurzor +2 → `i == 23`.

```cpp
    ++seq_;
    return i;
}
```

- `++seq_;` — a sorszám a **következő** keretre lép. `uint16_t`, ezért 65535
  után automatikusan 0-ra fordul (ez a kívánt körbefordulás).
- `return i;` — `i` most 23 = `kFrameSize`, a megírt bájtok száma.

A `Framer` tehát egy tiszta „adj telemetriát + puffert, megírom a keretet"
egység, futó sorszámmal, heap nélkül.

---

## 3. `include/channel.hpp`

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <random>

namespace satlink {

class NoisyChannel {
public:
    NoisyChannel(double bit_error_rate, uint32_t seed);

    void transmit(uint8_t* data, size_t len);

    void   set_bit_error_rate(double ber) { ber_ = ber; }
    double bit_error_rate() const { return ber_; }

    uint64_t total_bits() const { return total_bits_; }
    uint64_t flipped_bits() const { return flipped_bits_; }

private:
    double                                 ber_;
    std::mt19937                           rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
    uint64_t                               total_bits_   = 0;
    uint64_t                               flipped_bits_ = 0;
};

} // namespace satlink
```

- `#include <random>` — a véletlenmotorhoz és eloszláshoz (00/14).
- **Konstruktor** `NoisyChannel(double bit_error_rate, uint32_t seed);` — csak
  deklaráció itt; a teste a `.cpp`-ben. Két paraméter: a hibaarány és a mag.
- `void transmit(uint8_t* data, size_t len);` — **helyben** módosít egy
  bájttömböt (`uint8_t*`, nem `const`, mert írunk bele). Ez a „rontás".
- `set_bit_error_rate` / `bit_error_rate` — beállító és lekérdező a hibaarányra.
  A getter `const` (00/8). Inline-ok a fejlécben.
- `total_bits()` / `flipped_bits()` — `uint64_t` (64 bites!) számlálók
  lekérdezése. Azért 64 bites, mert milliószámra mennek a bitek, és egy 32 bites
  számláló túlcsordulhatna.
- Privát tagok:
  - `double ber_;` — a hibaarány. **Nincs** alapérték, mert a konstruktor
    úgyis beállítja (lásd lent).
  - `std::mt19937 rng_;` — a Mersenne-Twister motor (00/14). A konstruktor
    inicializálja a maggal.
  - `std::uniform_real_distribution<double> dist_{0.0, 1.0};` — a [0,1)
    egyenletes eloszlás, **kapcsos zárójeles** taginiciálással (00/6): a 0.0 és
    1.0 az eloszlás tartománya. Ez az alapérték a tag mellett, így a konstruktor
    listájában nem kell külön említeni.
  - `total_bits_ = 0`, `flipped_bits_ = 0` — nullázott számlálók (00/5).

A tagok **kiírásánál** (a `std::uniform_real_distribution<double>`) látod, miért
hasznos az `auto` máshol: a hosszú STL-típusnevek. Itt mezőként ki kell írni a
teljes típust.

---

## 4. `src/channel.cpp`

```cpp
#include "channel.hpp"
namespace satlink {

NoisyChannel::NoisyChannel(double bit_error_rate, uint32_t seed)
    : ber_(bit_error_rate), rng_(seed) {}
```

- A **konstruktor definíciója**, taginiciáló listával (00/8):
  - `ber_(bit_error_rate)` — a hibaarányt a paraméterből építi.
  - `rng_(seed)` — a motort a maggal **hozza létre**. Ez itt épül fel, nem
    később íródik felül — ezért a listában van.
  - A törzs `{}` üres: nincs több teendő.
  - A `dist_` tagot nem említi a lista, mert annak a fejlécben adtunk `{0.0,
    1.0}` alapértéket (00/6).

```cpp
void NoisyChannel::transmit(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            ++total_bits_;
            if (dist_(rng_) < ber_) {
                data[i] ^= static_cast<uint8_t>(1u << bit);
                ++flipped_bits_;
            }
        }
    }
}
```

- Külső ciklus: minden **bájton** (`size_t i`).
- Belső ciklus: minden bájt mind a **8 bitjén** (`int bit`).
- `++total_bits_;` — minden bitnél növeljük az össz-bitszámlálót.
- `if (dist_(rng_) < ber_)` — **itt dől el** a sors: a `dist_(rng_)` egy
  [0,1) közti véletlen szám (00/14); ha kisebb, mint a hibaarány (`ber_`), akkor
  ez a bit „balszerencsés". Pl. `ber_ = 0.001` esetén átlagosan minden 1000.
  bit billen.
- `data[i] ^= static_cast<uint8_t>(1u << bit);` — a billentés. Az `1u << bit`
  egy maszk, amiben csak a `bit`-edik bit 1 (00/16). A `^=` (XOR-és-visszaír)
  **átbillenti** pont azt a bitet (0↔1), a többit békén hagyja. A `1u` előjel
  nélküli literál (00/16), a `static_cast<uint8_t>` a `data[i]`-vel egyező
  típusra szűkít.
- `++flipped_bits_;` — számoljuk az átbillentett biteket (a dashboard ezt
  mutatja).

Ez a **hibainjektálás** lényege: szabályozható, megismételhető (a mag miatt),
hardver nélkül. Pontosan ez tesz egy ilyen projektet **tesztkörnyezetté**.

---

A keret megvan és el is tudjuk rontani. A [03-as fájl](03_dekoder.md) jön: a
vevő, ami ebből a (lehet, hogy sérült) bájtfolyamból visszafejti a kereteket — ez
a projekt magja.
