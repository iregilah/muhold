# 03 — A dekóder (a projekt magja)

Ez a vevő: egy bájtfolyamból, ami bárhol tartalmazhat szemetet, visszafejti a
kereteket. Egy **állapotgép**, ami **bájtonként**, **blokkolás és heap nélkül**
dolgozik — pont úgy, ahogy egy mikrokontroller UART-megszakítása hívná. Ezt a
részt érdemes legjobban érteni.

---

## 1. `include/decoder.hpp`

### Az állapot-enum

```cpp
enum class RxState {
    HuntSync,
    ReadHeader,
    ReadPayload,
    ReadCrc,
};
```

- `enum class` (00/4): a négy állapot típusbiztos halmaza. `RxState::HuntSync`
  formában hivatkozunk rájuk; nem keverhetők össze más enumokkal.
- Nincs explicit szám hozzájuk rendelve (0,1,2,3 lesz automatikusan), mert ezek
  **nem mennek a vezetékre** — csak belső állapot. Itt nem számít a konkrét
  érték, csak hogy megkülönböztethetők.

### A statisztika-struct

```cpp
struct DecoderStats {
    uint64_t bytes_seen     = 0;
    uint64_t frames_valid   = 0;
    uint64_t frames_crc_err = 0;
    uint64_t frames_dropped = 0;
    uint64_t resyncs        = 0;
};
```

- `struct`, mert buta számlálócsomag (00/5), minden mező **64 bites** (`uint64_t`)
  és **nullázott** alapértékkel (00/5). 64 bit, mert a kínzótesztben milliós
  nagyságrendű bájtszámok jönnek.
- Ez a dekóder **megfigyelhető kimenete** a telemetrián túl: a dashboard ezt
  rajzolja, a tesztek erre `CHECK`-elnek. Csak nőhetnek (monoton számlálók).

### Az osztály — publikus felület

```cpp
class Decoder {
public:
    bool feed(uint8_t byte);

    const Telemetry& last_packet() const { return last_; }
    uint16_t         last_seq() const { return last_seq_; }

    const DecoderStats& stats() const { return stats_; }
    RxState             state() const { return state_; }
```

- `bool feed(uint8_t byte);` — **a** központi metódus. Egy bájtot kap, és `true`-t
  ad **pontosan azon a bájton**, ami egy érvényes keretet lezárt. A többi lekérdező
  csak ezután érvényes adatra mutat.
- `const Telemetry& last_packet() const` — a legutóbb sikeresen dekódolt
  telemetria. **Referenciát ad vissza** (`const Telemetry&`), hogy ne másoljon, és
  a `const` (kétszer: a visszatérés is, a metódus is) garantálja, hogy a hívó nem
  módosíthatja a belső állapotot (00/8, 00/9).
- `last_seq()`, `stats()`, `state()` — további lekérdezők, mind `const`. A
  `stats()` a teljes számlálóblokkot adja vissza referenciaként.

### Az osztály — privát belső állapot

```cpp
private:
    void reset_to_hunt();

    RxState  state_   = RxState::HuntSync;
    uint32_t sync_sr_ = 0;

    uint8_t buf_[kHeaderSize + kMaxPayloadSize] = {};
    size_t  idx_ = 0;

    uint8_t  payload_len_ = 0;
    uint16_t seq_         = 0;
    uint16_t rx_crc_      = 0;
    uint8_t  crc_bytes_   = 0;

    Telemetry last_{};
    uint16_t  last_seq_      = 0;
    bool      have_last_seq_ = false;

    DecoderStats stats_{};
};
```

Minden privát, mert a külvilágnak csak a `feed()` + lekérdezők kellenek. A tagok:

- `void reset_to_hunt();` — privát segédmetódus (a teste a `.cpp`-ben).
- `RxState state_ = RxState::HuntSync;` — az **aktuális állapot**, kezdetben
  „szinkronkeresés". Alapérték a tag mellett (00/5).
- `uint32_t sync_sr_ = 0;` — a **görgő szinkronregiszter** (a kulcstrükk). 32
  bites, mert a SYNC 4 bájt = 32 bit. „sr" = shift register.
- `uint8_t buf_[kHeaderSize + kMaxPayloadSize] = {};` — a **fix méretű** puffer a
  fejléc + payload tárolására. Mérete `4 + 255 = 259` bájt: a **legrosszabb
  esetre** méretezve (00/22), sosem egy bemeneti hossz alapján. A `= {}`
  **nullázza** az egész tömböt induláskor (aggregátum-inicializálás, 00/6). Ez a
  „nincs heap" megkötés megtestesülése: a vevő összes munkaterülete egyetlen
  beépített tömb az objektumon belül.
- `size_t idx_ = 0;` — hány bájtot gyűjtöttünk eddig `buf_`-ba az aktuális
  keretből (index).
- `uint8_t payload_len_ = 0;` — a fejlécben **kapott** payload-hossz (validálás
  után mindig 13).
- `uint16_t seq_ = 0;` — a fejlécből kiolvasott **sorszám**.
- `uint16_t rx_crc_ = 0;` — a keretből **kapott** CRC (amit majd összevetünk a
  számolttal).
- `uint8_t crc_bytes_ = 0;` — hány CRC-bájtot gyűjtöttünk eddig (0, 1 vagy 2).
- `Telemetry last_{};` — a legutóbbi érvényes telemetria, **nullázva** (00/6).
- `uint16_t last_seq_ = 0;` — a legutóbbi érvényes keret sorszáma (a rés-
  detektáláshoz).
- `bool have_last_seq_ = false;` — volt-e már egyáltalán érvényes keret? Az első
  keretnél még nincs „előző", ezért nem számolunk rést.
- `DecoderStats stats_{};` — a számlálók, nullázva.

Figyeld meg: **egyetlen** `new`, `malloc`, `std::vector` sincs. Az egész vevő egy
fix méretű objektum. Ez a beágyazott alapelv (00/22).

---

## 2. `src/decoder.cpp`

### `reset_to_hunt` — visszatérés szinkronkeresésbe

```cpp
void Decoder::reset_to_hunt() {
    state_      = RxState::HuntSync;
    idx_        = 0;
    crc_bytes_  = 0;
    rx_crc_     = 0;
    // sync_sr_ szándékosan érintetlen marad
}
```

- Visszaáll `HuntSync`-be, és **nullázza** a per-keret munkaállapotot: az index,
  a CRC-bájtszámláló, a kapott CRC.
- **Nem** nullázza a `sync_sr_`-t. Ez tudatos: a görgő regiszter mindig az utolsó
  4 bájtot tartja, így ha rögtön a következő bájton új SYNC kezdődik (hát-hát
  pakolt keretek), azt is elkapjuk. Nullázni is biztonságos lenne, de így
  robusztusabb.

### `feed` — a teljes állapotgép

A függvény eleje:

```cpp
bool Decoder::feed(uint8_t byte) {
    ++stats_.bytes_seen;
```

- `++stats_.bytes_seen;` — minden hívásnál nő az össz-bájtszámláló. Ez az első
  dolog: bármi is történjen, ezt a bájtot „láttuk".

#### Folyamatos SYNC-detektálás (minden állapotban fut)

```cpp
    sync_sr_ = (sync_sr_ << 8) | byte;
    if (sync_sr_ == kSyncWord) {
        if (state_ != RxState::HuntSync) {
            ++stats_.resyncs;
        }
        state_     = RxState::ReadHeader;
        idx_       = 0;
        crc_bytes_ = 0;
        return false;
    }
```

Ez a **kulcs**. Soronként:

- `sync_sr_ = (sync_sr_ << 8) | byte;` — a regisztert **8 bittel balra toljuk**
  (a legrégebbi bájt kiesik a tetején), és az új bájtot **VAGY-oljuk** az alsó 8
  bitbe. Eredmény: a `sync_sr_` mindig pontosan az **utolsó 4 bájtot** tartja
  (00/16). Nincs külön puffer, nincs index — egyetlen egész szám.
- `if (sync_sr_ == kSyncWord)` — ha ez a 4 bájt épp a SYNC, **megtaláltuk** a
  keret elejét. És ez **minden állapotban** lefut, nem csak `HuntSync`-ben — ezért
  „folyamatos" detektálás.
- `if (state_ != RxState::HuntSync) ++stats_.resyncs;` — ha **nem** keresés
  közben találtuk a SYNC-et, az azt jelenti, hogy egy **félbehagyott** keretet
  dobtunk el, hogy az új SYNC-re álljunk → ez egy **resync** esemény.
- `state_ = RxState::ReadHeader; idx_ = 0; crc_bytes_ = 0;` — átlépünk a fejléc
  olvasásába, nullázott munkaállapottal. A **következő** bájttól kezdjük gyűjteni
  a fejlécet.
- `return false;` — ez a bájt (a SYNC utolsó bájtja) nem zár le érvényes keretet,
  tehát `false`.

**Miért robusztus ez?** Mert egy csonka vagy sérült keret sosem nyelheti el a
következő keret SYNC-jét: abban a pillanatban, ahogy egy valódi SYNC megjelenik,
újraindulunk rá. Egy rossz keret nem terjed több elveszett keretté. (A
kompromisszum — payload véletlenül tartalmazza a SYNC-mintát — ~1 a 4 milliárdhoz,
elhanyagolható; a kód kommentje részletezi.)

#### Az állapot szerinti elágazás

```cpp
    switch (state_) {

    case RxState::HuntSync:
        return false;
```

- Ha `HuntSync`-ben vagyunk, **nincs más teendő**: a SYNC-keresést már elvégezte
  a fenti rész. Csak várunk a következő bájtra. `return false`.

#### `ReadHeader` — a fejléc összegyűjtése és validálása

```cpp
    case RxState::ReadHeader: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize) {
            return false;
        }
```

- `buf_[idx_++] = byte;` — a bájtot a `buf_` aktuális pozíciójára írjuk, majd az
  indexet növeljük (utótag-növelés: előbb indexel, aztán léptet).
- `if (idx_ < kHeaderSize) return false;` — amíg nincs meg mind a 4 fejléc-bájt,
  visszatérünk. (`kHeaderSize == 4`.) A `{` a `case` után egy **blokk**, hogy a
  benne deklarált `version`, `seq_` stb. lokális hatókörben legyenek — `enum
  class`-os `switch`-nél ez a tiszta minta.

```cpp
        const uint8_t version = buf_[0];
        seq_         = get_u16(&buf_[1]);
        payload_len_ = buf_[3];
```

- A 4 bájt megvan, **kibontjuk**:
  - `version = buf_[0]` — a verziójelző (1 bájt).
  - `seq_ = get_u16(&buf_[1])` — a 2 bájtos sorszám big-endian olvasással
    (01-es fájl), a `buf_[1]`–`buf_[2]` bájtokból.
  - `payload_len_ = buf_[3]` — a hosszmező (1 bájt).
- `const uint8_t version` — lokális, nem változik, ezért `const`.

```cpp
        if (version != kVersionType || payload_len_ != kPayloadSize) {
            ++stats_.resyncs;
            reset_to_hunt();
            return false;
        }
```

- **Védekező validálás** (00/22): ha a verzió nem a miénk **vagy** a hossz nem 13,
  akkor ez nem értelmezhető keret (szemét, ami SYNC-et tartalmazott, vagy sérült
  fejléc). Eldobjuk: `resyncs++`, vissza `HuntSync`-be, `false`.
- A `||` rövidzár: ha a verzió már rossz, a hosszt meg sem nézi.
- **Miért fontos a hossz-ellenőrzés, ha úgyis mindig 13?** Mert ez a **szokás**:
  ha valaha a `payload_len_`-nel méreteznél puffert egy elrontott óriás
  értékből, az buffer overflow lenne. Itt megszokjuk, hogy a hosszt **validáljuk**,
  mielőtt bízunk benne.

```cpp
        state_ = RxState::ReadPayload;
        return false;
    }
```

- Ha a fejléc rendben, átlépünk `ReadPayload`-ba. Az `idx_` **marad** 4-en — a
  payload-bájtok a `buf_[4]`-től folytatódnak. `return false`.

#### `ReadPayload` — a payload összegyűjtése

```cpp
    case RxState::ReadPayload: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize + payload_len_) {
            return false;
        }
        state_     = RxState::ReadCrc;
        crc_bytes_ = 0;
        rx_crc_    = 0;
        return false;
    }
```

- `buf_[idx_++] = byte;` — a payload-bájtot a `buf_`-ba, index++.
- `if (idx_ < kHeaderSize + payload_len_) return false;` — amíg nincs meg a teljes
  fejléc+payload (`4 + 13 = 17` bájt), gyűjtünk tovább. Itt látszik, miért tartja
  a `buf_` a fejlécet is: a CRC majd a `buf_[0..16]`-ra számolódik.
- Ha megvan mind a 17 bájt: átlépünk `ReadCrc`-be, **nullázzuk** a CRC-gyűjtő
  állapotot (`crc_bytes_`, `rx_crc_`). `return false`.

#### `ReadCrc` — a CRC összerakása és ellenőrzése

```cpp
    case RxState::ReadCrc: {
        rx_crc_ = static_cast<uint16_t>((rx_crc_ << 8) | byte);
        if (++crc_bytes_ < kCrcSize) {
            return false;
        }
```

- `rx_crc_ = static_cast<uint16_t>((rx_crc_ << 8) | byte);` — a **kapott** CRC-t
  rakjuk össze big-endian: az első CRC-bájt a felső 8 bitre tolódik, a második
  alulra VAGY-olódik (00/16). A `static_cast<uint16_t>` a `int`-eredményt
  visszaszűkíti (00/10).
- `if (++crc_bytes_ < kCrcSize) return false;` — **előtag**-növeljük a
  számlálót, és ha még nincs meg mindkét bájt (`kCrcSize == 2`), visszatérünk.
  Az `++crc_bytes_` előbb növel, aztán hasonlít: első bájt után `1 < 2` igaz →
  `return`; második után `2 < 2` hamis → tovább.

```cpp
        const uint16_t calc = crc16_ccitt(buf_, kHeaderSize + payload_len_);
        if (calc == rx_crc_) {
```

- `crc16_ccitt(buf_, kHeaderSize + payload_len_)` — **újraszámoljuk** a CRC-t a
  bufferelt fejléc+payloadon (`buf_[0..16]`, 17 bájt). Ennek **egyeznie** kell az
  adóoldali számítással (02-es fájl), mert ott is a fejléc+payloadra számoltuk.
- `if (calc == rx_crc_)` — a számolt és a kapott CRC összevetése. Ha egyenlő, a
  keret **ép**.

```cpp
            last_ = deserialize_telemetry(&buf_[kHeaderSize]);
```

- A payloadot (`&buf_[4]`-től) **visszafejtjük** `Telemetry`-vé (01-es fájl), és
  eltároljuk `last_`-ba. A `kHeaderSize` (4) eltolás átugorja a fejlécet — a
  payload ott kezdődik.

```cpp
            if (have_last_seq_) {
                const uint16_t expected = static_cast<uint16_t>(last_seq_ + 1);
                if (seq_ != expected) {
                    const uint16_t gap = static_cast<uint16_t>(seq_ - expected);
                    stats_.frames_dropped += gap;
                }
            }
            last_seq_      = seq_;
            have_last_seq_ = true;
```

- **Sorszám-rés detektálás:**
  - `if (have_last_seq_)` — csak ha volt már előző érvényes keret (az elsőnél
    nincs mihez viszonyítani).
  - `expected = last_seq_ + 1` — a **várt** következő sorszám. `static_cast<uint16_t>`,
    mert a `+ 1` `int`-et ad, és a 16 bites körbefordulást akarjuk (65535+1 → 0).
  - `if (seq_ != expected)` — ha a kapott sorszám nem a várt, **rés** van.
  - `gap = seq_ - expected` — ennyi keret hiányzik. A `uint16_t` kivonás
    automatikusan kezeli a körbefordulást.
  - `stats_.frames_dropped += gap;` — hozzáadjuk az eldobott-számlálóhoz.
  - `last_seq_ = seq_; have_last_seq_ = true;` — frissítjük a „legutóbbi sorszám"
    állapotot a következő összevetéshez.

```cpp
            ++stats_.frames_valid;
            reset_to_hunt();
            return true;
        }
```

- `++stats_.frames_valid;` — egy érvényes kerettel több.
- `reset_to_hunt();` — vissza szinkronkeresésbe a következő keretre.
- `return true;` — **ez** a `true`: ez a bájt zárt le egy érvényes keretet. A
  hívó most kiolvashatja a `last_packet()`-et.

```cpp
        ++stats_.frames_crc_err;
        reset_to_hunt();
        return false;
    }
    }

    return false;
}
```

- Ha a CRC **nem** egyezett: `frames_crc_err++`, vissza `HuntSync`-be, `false`.
  Telemetriát **nem** adunk ki (a sérült payloadban nem bízunk).
- A dupla `}` zárja a `case` blokkot és a `switch`-et.
- A záró `return false;` elvileg **elérhetetlen** (minden `case` visszatér), de a
  fordító enélkül figyelmeztetne, hogy „nem minden úton van return". Tiszta lelkiismeret.

---

## 3. Hogyan áll össze — egy ép keret útja

1. A SYNC 4 bájtja begörög a `sync_sr_`-be → `== kSyncWord` → `ReadHeader`.
2. 4 fejléc-bájt a `buf_[0..3]`-ba → validálás → `ReadPayload`.
3. 13 payload-bájt a `buf_[4..16]`-ba → `ReadCrc`.
4. 2 CRC-bájt összeáll `rx_crc_`-be → újraszámolt CRC összevetése → ha egyezik:
   `deserialize`, `frames_valid++`, `return true`.

Egy **sérült** keret ugyanígy halad, de a 4. lépésben a CRC nem egyezik →
`frames_crc_err++`, `false`. Egy **csonka** keret esetén a következő valódi SYNC
a folyamatos detektálással bármikor újraindít. Így a gép **sosem ragad be**, és
egy rossz keret nem visz magával többet.

---

A vevő kész. A [04-es fájl](04_szimulator_es_kijelzo.md) a telemetria-forrást és a
látványos kijelzőt elemzi.
