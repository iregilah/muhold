# 01 — A protokoll-réteg: protocol.hpp, serialization, crc16

Ez a réteg az „adat a vezetéken" definíciója és a hozzá tartozó alacsony szintű
átalakítások. Soronként végigmegyünk. A nyelvi elemekhez a [00-s
alapozóra](00_nyelvi_alapozo.md) hivatkozom (pl. „lásd 00/3" = a constexpr-pont).

---

## 1. `include/protocol.hpp`

### Fejléc-rész

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace satlink {
```

- `#pragma once` — egyszeri behúzás (00/1).
- `<cstddef>` — innen jön a `size_t` (00/2).
- `<cstdint>` — innen a `uint8_t`, `uint16_t`, `uint32_t`, `int16_t` (00/2).
- `namespace satlink {` — minden projektkód ebben a névtérben él (00/7). A fájl
  legalján `}` zárja.

### A SYNC és a verzió

```cpp
constexpr uint32_t kSyncWord = 0x1ACFFC1D;
constexpr uint8_t kVersionType = 0x01;
```

- `constexpr` (00/3): fordítási idejű állandók, típussal. A `k` előtag a
  konstans-konvenció.
- `0x1ACFFC1D` egy **32 bites** hexa literál — pont elfér egy `uint32_t`-ben (4
  bájt). Ez a valódi CCSDS szinkronjel.
- `0x01` egy **1 bájtos** verzióazonosító. Ennyi a fejléc első bájtja.

### Méret-konstansok

```cpp
constexpr size_t kSyncSize    = 4;
constexpr size_t kHeaderSize  = 4;   // version_type(1) + seq_count(2) + payload_len(1)
constexpr size_t kPayloadSize = 13;
constexpr size_t kCrcSize     = 2;

constexpr size_t kFrameSize = kSyncSize + kHeaderSize + kPayloadSize + kCrcSize; // 23
constexpr size_t kCrcCoveredSize = kHeaderSize + kPayloadSize; // 17
constexpr size_t kMaxPayloadSize = 255;
```

- Minden méret `size_t` (00/2) és `constexpr` — mert ezekből **tömbméreteket**
  és **fordítási idejű** számításokat csinálunk, ahol kötelező a fordítási idejű
  állandó.
- `kFrameSize` egy másik `constexpr`-ekből **számolt** `constexpr`. Ez a
  `constexpr` ereje: a `4 + 4 + 13 + 2 = 23` a fordítónál dől el, nem futáskor.
  Ezt használjuk később `uint8_t frame[kFrameSize];` méretként.
- `kCrcCoveredSize` (17) **kiemelt** konstans, mert a CRC-lefedés a leggyakoribb
  hibaforrás — saját nevet kapott, hogy egyértelmű legyen.
- `kMaxPayloadSize = 255` — a **védekező** felső korlát: a vevő ekkora payloadra
  méretezi a puffert, sosem egy bemeneti hossz alapján (00/22).

### Az üzemmód-enum

```cpp
enum class Mode : uint8_t {
    Safe    = 0,
    Nominal = 1,
    Payload = 2,
    Comms   = 3,
};
```

- `enum class` (00/4): hatókörös, nem konvertálódik magától egésszé.
- `: uint8_t` — az alaptípus **1 bájt**, mert a vezetéken 1 bájt a mode.
- A `= 0,1,2,3` explicit értékek: a vezetéken ezek a számok mennek, ezért
  rögzítjük őket (nem bízzuk a fordítóra). A `Safe = 0` szándékos: a nullázott
  alapállapot a legbiztonságosabb mód.

### A telemetria-rekord

```cpp
struct Telemetry {
    uint32_t timestamp_ms  = 0;
    uint16_t batt_mv       = 0;
    int16_t  temp_eps_c10  = 0;
    int16_t  temp_obc_c10  = 0;
    int16_t  attitude_cdeg = 0;
    Mode     mode          = Mode::Safe;
};
```

- `struct`, mert ez **buta adatcsomag** (00/5): csak mezők, nincs viselkedés.
- Minden mezőnek **alapértelmezett tagértéke** van (00/5) — `= 0`, illetve
  `= Mode::Safe`. Így egy `Telemetry t{};` mindig definiált, nullázott (00/6).
- **Fix szélességű** típusok, mert ezek mennek a vezetékre (00/2).
- **Előjeles** mezők (`int16_t`) ott, ahol a fizikai mennyiség lehet negatív
  (hőmérséklet, helyzetszög), **előjel nélküli** (`uint16_t`) ahol nem
  (feszültség mindig ≥ 0).
- A skálázás a **mezőnévben** kódolva: `c10` = 0.1 °C-os lépés, `cdeg` = 0.01°-os
  lépés. Miért skálázott egész és nem `float`? Lásd 00/16 szellemében: kompakt,
  determinisztikus, nincs lebegőpontos pontatlanság a vezetéken. A `cdeg`
  (centifok) választás oka, hogy a ±180° **elférjen** egy `int16_t`-ben
  (±32767): centifokban ±18000 belefér, millifokban (±180000) nem.

---

## 2. `include/serialization.hpp` — a deklarációk

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "protocol.hpp"

namespace satlink {

void     put_u16(uint8_t* p, uint16_t v);
void     put_u32(uint8_t* p, uint32_t v);
void     put_i16(uint8_t* p, int16_t v);
uint16_t get_u16(const uint8_t* p);
uint32_t get_u32(const uint8_t* p);
int16_t  get_i16(const uint8_t* p);

size_t    serialize_telemetry(const Telemetry& t, uint8_t* out);
Telemetry deserialize_telemetry(const uint8_t* in);

} // namespace satlink
```

- `#include "protocol.hpp"` — kell, mert a `Telemetry` itt szerepel. (Idézőjeles
  include = saját fejléc; `<>` = rendszer/könyvtári.)
- A `put_*`/`get_*` a **pointeres**, alacsony szintű réteg: nyers bájtokra
  mutatnak (`uint8_t*` írásra, `const uint8_t*` olvasásra — 00/9). Nincs hossz,
  mert mindegyik **fix** számú bájtot ír/olvas (a nevében benne van: `u16` = 2
  bájt).
- `serialize_telemetry`: a `Telemetry`-t `const&`-ként veszi (nem másol, nem
  módosít — 00/9), és egy `out` pufferbe ír. `size_t`-et ad vissza: hány bájtot
  írt (mindig `kPayloadSize`).
- `deserialize_telemetry`: a fordítottja — egy `Telemetry`-t **értékként ad
  vissza** (a struct kicsi, a másolás olcsó, és tisztább interfész).

A fejléc csak **megígéri**, hogy ezek a függvények léteznek; a testük a `.cpp`-ben
van (00/1).

---

## 3. `src/serialization.cpp` — a megvalósítás

### `put_u16` / `put_u32` — big-endian írás

```cpp
void put_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);  // magas bájt elöl
    p[1] = static_cast<uint8_t>(v & 0xFF);         // alacsony bájt
}
```

Soronként:
- `v >> 8` — a 16 bites értéket **8 bittel jobbra** toljuk, így a **felső** bájt
  kerül az alsó 8 bitbe (00/16).
- `& 0xFF` — maszkoljuk az alsó 8 bitre (a felső bitek nullázódnak).
- `static_cast<uint8_t>(...)` — a kifejezés `int`-té léptetődik elő (00/16), ezért
  kimondjuk a `uint8_t`-be szűkítést, hogy ne figyelmeztessen a fordító (00/10).
- `p[0]` a **magas** bájt, `p[1]` az **alacsony** → ez a **big-endian**: a
  legértékesebb bájt a legkisebb címen. Ezt diktálja a hálózati/űr-szabvány.

A `put_u32` ugyanez 4 bájtra (`>> 24`, `>> 16`, `>> 8`, majd alsó):

```cpp
void put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}
```

### `put_i16` — előjeles írás újrahasznosítással

```cpp
void put_i16(uint8_t* p, int16_t v) {
    put_u16(p, static_cast<uint16_t>(v));
}
```

- Egy `int16_t` **bitmintája** kettes komplemensben azonos az ugyanazt a mintát
  hordozó `uint16_t`-ével. Ezért elég `uint16_t`-re castolni és a meglévő
  `put_u16`-ot hívni. A negatív érték bitjei sértetlenül kerülnek a vezetékre.

### `get_u16` / `get_u32` — big-endian olvasás

```cpp
uint16_t get_u16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                 static_cast<uint16_t>(p[1]));
}
```

- A `p[0]`-t (magas bájt) **8 bittel balra** toljuk, a `p[1]`-et (alacsony)
  hozzá-VAGY-oljuk → visszaáll a 16 bites érték (00/16).
- A belső `static_cast<uint16_t>(p[0])`: a `p[0]` `uint8_t`, a `<< 8` előtt
  `int`-té léptetődne; explicit `uint16_t`-re emeljük, hogy a tolás biztosan 16
  biten történjen, és a végén a külső cast a `|` `int`-eredményét visszaszűkíti
  `uint16_t`-re (00/10, 00/16). Ez a sok cast a „helyes, figyelmeztetésmentes
  bitmunka" ára.
- `const uint8_t* p` — **olvasunk**, nem írunk, ezért `const` (00/9).

A `get_u32` ugyanez 4 bájtra, `<< 24 | << 16 | << 8 | ...` mintával.

### `get_i16` — előjeles olvasás

```cpp
int16_t get_i16(const uint8_t* p) {
    return static_cast<int16_t>(get_u16(p));
}
```

- Összerakjuk a 16 bitet **előjel nélkül**, majd `int16_t`-re castoljuk. A
  bitminta változatlan; a cast „újraértelmezi" előjelesként. (C++20 óta
  garantáltan, korábban minden valódi fordítón így működik — ezért biztonságos.)

### A teljes rekord szerializálása

```cpp
size_t serialize_telemetry(const Telemetry& t, uint8_t* out) {
    put_u32(&out[0], t.timestamp_ms);
    put_u16(&out[4], t.batt_mv);
    put_i16(&out[6], t.temp_eps_c10);
    put_i16(&out[8], t.temp_obc_c10);
    put_i16(&out[10], t.attitude_cdeg);
    out[12] = static_cast<uint8_t>(t.mode);
    return kPayloadSize;
}
```

- **`&out[0]`, `&out[4]`, ...** — a puffer adott **eltolásának címe**. A
  `&out[4]` pontosan `out + 4` (pointer-aritmetika, amit ismersz): a 4. bájtra
  mutató pointer. Innen ír a `put_u16` 2 bájtot.
- Az **eltolások** (`0, 4, 6, 8, 10, 12`) pontosan a payload-mezőhatárok:
  timestamp 4 bájt (0–3), batt 2 (4–5), 3× temp/att 2-2 (6–7, 8–9, 10–11), mode 1
  (12). Összesen 13 = `kPayloadSize`.
- `out[12] = static_cast<uint8_t>(t.mode)` — a `Mode` `enum class`, ezért a
  számmá alakítást **ki kell mondani** (00/4). 1 bájt, közvetlen írás (nincs
  bájtsorrend egyetlen bájtnál).
- `return kPayloadSize;` — a hívónak jelzi, hány bájtot írtunk.

### A teljes rekord visszafejtése

```cpp
Telemetry deserialize_telemetry(const uint8_t* in) {
    Telemetry t;
    t.timestamp_ms  = get_u32(&in[0]);
    t.batt_mv       = get_u16(&in[4]);
    t.temp_eps_c10  = get_i16(&in[6]);
    t.temp_obc_c10  = get_i16(&in[8]);
    t.attitude_cdeg = get_i16(&in[10]);
    t.mode          = static_cast<Mode>(in[12]);
    return t;
}
```

- Pontosan a `serialize` **tükörképe**: ugyanazok az eltolások, ugyanazok a
  típusok. A kettőt mindig **együtt** kell módosítani — ezért vannak egymás
  mellett.
- `static_cast<Mode>(in[12])` — a bájtból `enum class`-t csinálunk (00/4). Ha a
  bájt érvénytelen mode-érték lenne (pl. 7), az enum „ismeretlen" értéket kapna;
  a dashboard ezt `UNKNOWN`-ként kezeli.
- `Telemetry t;` majd `return t;` — a kicsi struct **érték szerint** tér vissza
  (a fordító ezt jellemzően „elmásolás nélkül", helyben optimalizálja).

---

## 4. `include/crc16.hpp`

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
namespace satlink {
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
}
```

- Egyetlen függvény: kap egy **bájttömböt** (`const uint8_t* data`, olvasás) és
  egy **hosszt** (`size_t len`), visszaad egy **16 bites** CRC-t. Ez a klasszikus
  „pointer + hossz" C-interfész egy memóriablokkra.

---

## 5. `src/crc16.cpp` — a CRC kiszámítása

```cpp
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    constexpr uint16_t kPoly = 0x1021;
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ kPoly);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}
```

Soronként, ez a leginkább „bit-szintű" függvény:

- `constexpr uint16_t kPoly = 0x1021;` — a **polinom**, a CRC-CCITT szabvány
  paramétere. Függvényen belüli `constexpr` (00/3): csak itt látszik.
- `uint16_t crc = 0xFFFF;` — a **kezdőérték** (a CCITT-FALSE variáns minden bitet
  1-re állít induláskor).
- Külső ciklus: `for (size_t i = 0; i < len; ++i)` — végigmegy minden bájton. A
  `size_t` a hosszhoz illik (00/2).
- `crc ^= static_cast<uint16_t>(data[i]) << 8;` — a soron következő bájtot a
  16 bites regiszter **felső** bájtjába XOR-oljuk be. A `data[i]` `uint8_t`,
  ezért `<< 8` előtt `uint16_t`-re emeljük (00/16), különben elveszne a tolás. Az
  `^=` a „XOR és visszaír" rövidítés.
- Belső ciklus: `for (int bit = 0; bit < 8; ++bit)` — a bájt mind a 8 bitjét
  feldolgozza, MSB-től.
  - `if (crc & 0x8000)` — a `0x8000` maszk a **legfelső** (15.) bitet teszteli.
    Ha az 1, akkor a tolásnál „kibukik".
  - Igen ág: `crc = static_cast<uint16_t>((crc << 1) ^ kPoly);` — egyet balra
    told, majd XOR a polinommal. Ez a polinom-osztás egy lépése (GF(2)).
  - Nem ág: `crc = static_cast<uint16_t>(crc << 1);` — csak told egyet balra.
  - Mindkét ágban `static_cast<uint16_t>(...)`, mert a `<<` `int`-et ad (00/16),
    és a felső bitek „lelógását" a 16 bitre szűkítéssel vágjuk le.
- `return crc;` — a maradék a CRC.

Miért bitenként és nem táblázattal? Mert így **átlátszó**: pontosan látod a
polinom-osztást. Gyorsabb (táblázatos) változat is van, de tanuláshoz és ekkora
adatmennyiséghez ez tökéletes. A `"123456789" → 0x29B1` ismert ellenőrzőértéket a
[teszt](05_fociklus_es_tesztek.md) bizonyítja — ezzel igazolható, hogy a
megvalósítás szabványos.

---

Ezzel megvan az „adat-bájtok" réteg. A [02-es fájl](02_ado_oldal.md) megmutatja,
hogyan rak ebből a `Framer` teljes keretet, és hogyan rontja el a `NoisyChannel`.
