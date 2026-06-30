# Hogyan működik — részletes magyarázat

Ez a dokumentum végigvezet a projekt minden részén, magyarul, hogy pontosan
értsd, mit csinál a kód és **miért** úgy csinálja. A sorrend alulról fölfelé
halad: előbb az építőkockák, aztán a dekóder állapotgép (a lényeg), végül a
megjelenítés és a tesztek.

---

## 0. Az egész egy mondatban

Egy szimulált műhold telemetriát (akkufeszültség, hőmérsékletek, helyzetszög)
**csomagokba keretez**, átküldi egy **zajos rádiócsatornán** (ami véletlenül
bitet billent), a vevőoldal pedig **bájtonként, egy állapotgéppel** visszafejti,
**CRC-vel ellenőrzi**, és egy **élő terminálkijelzőn** mutatja az értékeket és a
hibastatisztikát.

A cső (pipeline):

```
Satellite ──► Framer ──► NoisyChannel ──► Decoder ──► Dashboard
 (érzékelők)  (keretez)  (bithibák)       (állapot-   (élő kijelző)
                                           gép)
```

Ez pontosan az a felépítés, ahogy egy földi állomás szoftvere ülne egy valódi
rádiólink végén. Minden modulnak van egy `.hpp` (deklaráció, „mit tud") és egy
`.cpp` (definíció, „hogyan csinálja") fájlja — ez a C++ projektek szokásos
szerkezete, és beágyazott környezetben is így néz ki.

---

## 1. A protokoll — `include/protocol.hpp`

Ez a fájl az **egyetlen igazságforrás** arról, hogy egy keret hogyan néz ki a
vezetéken. Az adó és a vevő ebben kell megegyezzen, különben a bájtokból nem
ugyanaz jön ki a két oldalon.

Egy keret 23 bájt. Minden többbájtos mező **big-endian** (a legértékesebb bájt
elöl — ez a hálózati és űr-szabványok bájtsorrendje):

```
Eltolás Méret Mező           Érték / jelentés          CRC fedi?
------  ----- -------------  ------------------------  ---------
  0      4    SYNC           0x1A 0xCF 0xFC 0x1D       NEM
  4      1    version_type   0x01                      igen
  5      2    seq_count      keretenként nő            igen
  7      1    payload_len    13                        igen
  8     13    payload        szerializált Telemetry    igen
 21      2    crc16          CRC-16/CCITT-FALSE        — (ez maga a CRC)
```

A legfontosabb részlet, amit könnyű elrontani: **a CRC a 4..20 bájtokra
(fejléc + payload, 17 bájt) számolódik.** A SYNC nincs benne, és maga a CRC-mező
sincs benne. Ha ezt elvéted, minden keret elhasal a CRC-n.

A `SYNC` értéke a valódi CCSDS „Attached Sync Marker": `0x1ACFFC1D`. Ez nem
véletlen szám — ezt használják igazi műholdaknál a keret kezdetének jelölésére.

### A telemetria-rekord

```cpp
struct Telemetry {
    uint32_t timestamp_ms;   // ms a bekapcsolás óta (RTC-ből)
    uint16_t batt_mv;        // akkufeszültség, mV
    int16_t  temp_eps_c10;   // EPS-hőmérséklet, 0.1 °C lépésben
    int16_t  temp_obc_c10;   // OBC-hőmérséklet, 0.1 °C lépésben
    int16_t  attitude_cdeg;  // helyzetszög, 0.01 ° lépésben
    Mode     mode;           // üzemmód (1 bájt)
};
```

Figyeld meg: **nincs `float`.** A vezetéken kompakt, skálázott egészeket
küldünk. A `temp_eps_c10 = 235` azt jelenti, hogy 23.5 °C. Ez beágyazott
szokás: egy ADC nyers számát csak a kijelzésnél váltjuk mérnöki egységgé, mert
az egészek kisebbek, gyorsabbak, és nincs lebegőpontos hiba.

Egy tudatos tervezési döntés: a helyzetszöget **0.01 ° (centifok)** lépésben
tároljuk, nem millifokban. Miért? A teljes ±180°-os tartomány millifokban
±180000 lenne, ami **nem fér el** egy előjeles 16 bites mezőben (az csak
±32767-ig megy). Centifokban ±18000, ami bőven befér. Ez pont az a fajta
megfontolás — a skálát a mező szélességéhez igazítani —, ami valódi beágyazott
munkában is felmerül.

---

## 2. Big-endian szerializáció — `serialization.{hpp,cpp}`

**Miért nem lehet egyszerűen a struktúrát a vezetékre másolni (`memcpy`)?** Két
okból:

1. A fordító **kitöltő bájtokat** (padding) tehet a mezők közé az igazítás
   miatt — ezek szemét bájtok lennének a vezetéken.
2. A bájtsorrend a CPU-tól függ. Egy x86 **little-endian** (a kis bájt elöl),
   egy másik chip lehet big-endian. Ha nyersen másolod, a két gép **mást
   értene** ugyanazon a bájtsorozaton.

A megoldás: mezőnként, kézzel, megegyezett bájtsorrendben írjuk ki. A két
alapfüggvény:

```cpp
void put_u16(uint8_t* p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;  // magas bájt elöl  (big-endian)
    p[1] =  v       & 0xFF;  // alacsony bájt
}
uint16_t get_u16(const uint8_t* p) {
    return (p[0] << 8) | p[1];  // visszarakjuk ugyanúgy
}
```

Az **előjeles** mezőkre figyelni kell: a 16 bitet előbb `uint16_t`-ként rakjuk
össze, majd `int16_t`-re castoljuk. Két komplemensben ugyanaz a bitminta, ezért
a negatív hőmérséklet helyesen jön vissza. A tesztben ezt külön ellenőrizzük
(`put_i16(p, -1)` → `0xFF 0xFF`).

A `serialize_telemetry` és `deserialize_telemetry` a 13 bájtos payloadot
mezőnként pakolja ki/be. **A két függvénynek pontosan tükröznie kell egymást** —
ezért vannak egymás mellett a fájlban, és együtt kell őket szerkeszteni.

---

## 3. CRC-16 — `crc16.{hpp,cpp}`

A **CRC (ciklikus redundancia-ellenőrzés)** egy rövid ellenőrző összeg, amit az
adó kiszámol az adatra és melléküld. A vevő újraszámolja a kapott adatra, és
összeveti. Ha akár egyetlen bit is megváltozott útközben, az újraszámolt érték
szinte biztosan más lesz → a keretet eldobjuk.

A mi variánsunk: **CRC-16/CCITT-FALSE** (polinom `0x1021`, kezdőérték `0xFFFF`,
nincs bit-tükrözés, nincs záró XOR). Ennek ismert „check value"-ja van: a
`"123456789"` sztringre `0x29B1`. Ezt a teszt ellenőrzi, és bármely online CRC
kalkulátorral összevetheted — így biztos lehetsz benne, hogy a megvalósítás jó.

Hogyan működik a számítás (bitenkénti változat)?

```cpp
uint16_t crc = 0xFFFF;
for (minden bájt) {
    crc ^= bájt << 8;          // a bájtot a regiszter tetejére visszük
    for (8 bit) {
        if (crc & 0x8000)      // ha a legfelső bit 1,
            crc = (crc<<1) ^ 0x1021;  // eltol + XOR a polinommal
        else
            crc = crc<<1;             // különben csak eltol
    }
}
```

Ez lényegében **polinom-osztás kettes számrendszerben** (GF(2)). Nem kell a
matekot mélyen érteni a használathoz — a lényeg: determinisztikus, és a
bithibákra rendkívül érzékeny. Egyetlen átbillent bit más maradékot ad.

---

## 4. Keretező — `framer.{hpp,cpp}`

Ez az **adóoldal**: egy `Telemetry`-ből összerak egy teljes keretet. A
`Framer` osztály egy futó `seq_` számlálót tart, amit minden keretnél növel — a
vevő ebből veszi észre, ha keret veszett el.

```cpp
size_t Framer::encode(const Telemetry& t, uint8_t* out, size_t out_cap) {
    if (out_cap < kFrameSize) return 0;   // sose írj a puffer fölé
    put_u32(out+0, kSyncWord);            // SYNC (CRC nélkül)
    // fejléc:
    out[4] = kVersionType;
    put_u16(out+5, seq_);
    out[7] = kPayloadSize;
    // payload:
    serialize_telemetry(t, out+8);
    // CRC a 4..20 bájtokra:
    put_u16(out+21, crc16_ccitt(out+4, kCrcCoveredSize));
    ++seq_;
    return kFrameSize;
}
```

Fontos beágyazott minta: **a hívó adja a puffert** (`out`), nem heap-vektort
adunk vissza. Pontosan így működne egy igazi adóút, ahol az OBC egy bájtpuffert
ad át a rádió UART-jának. És sose írunk a puffer kapacitása fölé.

---

## 5. Zajos csatorna — `channel.{hpp,cpp}`

Ez modellezi a rádiólinket. Minden áthaladó bitet adott valószínűséggel (a
**bithiba-arány**, BER) átbillent:

```cpp
void NoisyChannel::transmit(uint8_t* data, size_t len) {
    for (minden bájt)
        for (8 bit) {
            ++total_bits_;
            if (random[0,1) < ber_) {       // ez a bit balszerencsés
                data[i] ^= (1u << bit);      // átbillentjük
                ++flipped_bits_;
            }
        }
}
```

Ez **maga a tesztkörnyezet** lényege: szabályozott hibainjektálás, amivel
bizonyítható, hogy a vevő elkapja a sérülést és helyreáll — valódi hardver és
rádió nélkül. A magot (a véletlent) seed-eljük, így a futások megismételhetők.

---

## 6. A dekóder állapotgép — `decoder.{hpp,cpp}` — **EZ A LÉNYEG**

Ez a projekt szíve, és ez készít fel leginkább a beágyazott munkára. A dekóder
**egyetlen bájtot kap egyszerre** (`feed(byte)`), pontosan úgy, ahogy egy UART
megszakítás adná. **Nem blokkol és nem allokál** — minden puffer fix méretű
osztálytag. Ettől lesz ugyanez a kód használható egy valódi mikrokontroller
megszakításrutinjában.

### Bemenet és kimenet

- **Bemenet:** bájtfolyam, egyenként `feed()`-en át. A folyamban bárhol lehet
  szemét: keretek előtt, között, akár „bennük" is (ha egy keret csonka lett).
- **Kimenet:** a `feed()` **`true`-t ad pontosan azon a bájton, amelyik egy
  érvényes (CRC-helyes) keretet lezárt**; ilyenkor a `last_packet()` adja a
  visszafejtett `Telemetry`-t. Emellett egy `DecoderStats` számlálóblokk
  mutatja, mi történt (érkezett bájt, érvényes keret, CRC-hiba, eldobott,
  resync).

### A négy állapot

```
HUNT_SYNC → READ_HEADER → READ_PAYLOAD → READ_CRC
    ▲────────────┴──────────────┴─────────────┘
       (bármilyen hiba → vissza HUNT-ba)
```

1. **HUNT_SYNC** — a szinkronszót keressük a folyamban.
2. **READ_HEADER** — összegyűjtjük a 4 fejléc-bájtot, és **védekezően**
   ellenőrizzük (verzió, hossz). Hibás → eldobjuk, resync.
3. **READ_PAYLOAD** — összegyűjtjük a `payload_len` payload-bájtot.
4. **READ_CRC** — összegyűjtjük a 2 CRC-bájtot, újraszámoljuk a CRC-t a
   bufferelt fejléc+payloadra, és összevetjük. Egyezik → érvényes keret;
   nem → CRC-hiba. Mindkét esetben vissza HUNT-ba.

### A görgő szinkronregiszter (a kulcstrükk)

Ahelyett, hogy 4 bájtot tárolnánk és minden eltolásnál `memcmp`-pel
hasonlítgatnánk, **egyetlen 32 bites regisztert** tartunk. Minden beérkező
bájtra balra told 8-cal és OR-old be az újat:

```cpp
sync_sr_ = (sync_sr_ << 8) | byte;
if (sync_sr_ == kSyncWord) { /* megvan a keret eleje */ }
```

Így a regiszterben **mindig az utolsó 4 bájt** ül. A szinkronszó bármelyik
bájthatáron felbukkanhat a szemét közepén is — a görgő regiszter ezt ingyen
kezeli, nincs indexbűvészkedés.

### Folyamatos SYNC-detektálás (a robusztus változat)

Egy fontos tervezési döntés: a SYNC-keresés **minden állapotban fut**, nem csak
HUNT-ban. A `feed()` legelején minden bájtot begörgetünk a regiszterbe, és ha
SYNC-et látunk, **bármelyik állapotból** újraindulunk a fejléc olvasásával:

```cpp
sync_sr_ = (sync_sr_ << 8) | byte;
if (sync_sr_ == kSyncWord) {
    if (state_ != HUNT_SYNC) ++stats_.resyncs;  // félbehagyott keretet dobtunk
    state_ = READ_HEADER; idx_ = 0; ...
    return false;
}
```

**Miért ez a jó?** Tegyük fel, hogy egy keret csonka lett (megszakadt a link).
A naiv vevő tovább gyűjtené a payloadot, és közben „elnyelné" a **következő**
keret SYNC-jét is — így két keret veszne el egy helyett. A folyamatos
detektálással abban a pillanatban, ahogy egy valódi SYNC megjelenik,
újraszinkronizálunk. Egy rossz keret **nem terjed tovább** több elveszett
keretté. Ez pont a `tests/` „Resync after a truncated frame" tesztjét teszi
egyértelművé.

**A kompromisszum** (őszintén): ha egy payload véletlenül tartalmazná a 4 bájtos
SYNC-mintát, az hamis újraindítást okozna. 32 bites jelnél ez kb. 1 a 4
milliárdhoz minden bájtpozícióra — elhanyagolható. Valódi rendszerek ezt fix
keret-pozícióval vagy bájt-stuffinggal végképp kizárják.

### Védekező feldolgozás

A fejléc beolvasása után, **mielőtt bármit elhinnénk**, ellenőrzünk:

```cpp
if (version != kVersionType || payload_len_ != kPayloadSize) {
    ++stats_.resyncs; reset_to_hunt(); return false;
}
```

Most a hossz fix (13), de a **szokást** építjük be: ha valaha a `payload_len`-nel
méreteznél puffert és megbíznál egy elrontott óriás értékben, az **buffer
overflow** lenne — pont az a fajta hiba, ami repülő szoftverben végzetes. Ezért
a `buf_` eleve a **maximális** keretre van méretezve (`kHeaderSize +
kMaxPayloadSize`), sosem egy bemeneti hossz alapján.

### Sorszám-rés (dropped) detektálás

Amikor egy érvényes keret megérkezik, megnézzük, hogy a `seq_count` a várt
következő-e. Ha ugrott, annyi keret veszett el a linken:

```cpp
uint16_t expected = last_seq_ + 1;
if (seq_ != expected) stats_.frames_dropped += (seq_ - expected);
```

Megjegyzés a „Dropped" és „CRC errors" viszonyához: egy CRC-n elbukott keret a
payloadja miatt eldobódik (nem fejtjük vissza), így a **következő** jó keret egy
sorszám-rést lát → ezért egy sérült keret jellemzően **mind** a „CRC errors",
**mind** a „Dropped" számlálót növeli. A „Dropped" tehát a teljes hiányt méri a
sorszám-folyamban (a CRC-bukott + a teljesen elveszett keretek együtt). Ez
szándékos és informatív, nem hiba.

### A „repülésbiztos" tulajdonságok összefoglalva

- **Sose ragad be:** minden keret után (jó vagy rossz) vissza HUNT-ba, és egy
  SYNC bármikor újraindít.
- **Nincs allokáció, nincs blokkolás:** fix pufferek, bájtonkénti feldolgozás.
- **Nem bízik a bemenetben:** verzió/hossz ellenőrzés, max-méretű puffer.

---

## 7. Műhold-szimulátor — `satellite.{hpp,cpp}`

A valódi érzékelők helyett szinuszokkal állít elő élethű, lassan változó
értékeket a küldetési idő függvényében: az akku tölt-merül egy „pálya" alatt, a
hőmérsékletek napsütés/árnyék szerint ingadoznak, a helyzet lassan forog, az
üzemmód pedig a tápállapotot követi (alacsony akku → SAFE). Egy kis
mérési zajt is keverünk hozzá, hogy a kijelző „élőnek" tűnjön. A magot
seed-eljük, így megismételhető.

---

## 8. Dashboard — `dashboard.{hpp,cpp}`

Ez a **látványos** rész: egy teljes képernyős ANSI-kijelző, ami helyben frissül.
Trükkök:

- A képernyő törlése és a kurzor elrejtése induláskor (`\x1b[2J\x1b[?25l`),
  minden rendernél a kurzort a bal felső sarokba visszük (`\x1b[H`), és minden
  sort a végéig törlünk (`\x1b[K`). Így **villogásmentesen** újrarajzol.
- A mértékek (gauge-ek) Unicode blokk-karakterekből (`█`, `░`) épülnek.
- A színek (zöld/sárga/piros) a küszöbök szerint váltanak — pl. az akku zöld,
  ha tele, piros, ha kritikus; a CRC-hiba számláló pirosban, ha nem nulla.

Egy fontos részlet az igazításnál: a doboz **abszolút oszlopokhoz** igazít.
Néhány karakter (`°`, `✓`, `✗`, a blokkok) UTF-8-ban **több bájt, de egy oszlop
széles**. Ezért külön számon tartjuk a *látható* oszlopszélességet, nem a bájtok
számát — különben a jobb szegély elcsúszna. (Pont ezt a hibát kaptuk el
fejlesztés közben: a `✓` 3 bájt, és bájtként számolva 2 oszloppal rövidült a
sor.)

---

## 9. A fő ciklus — `src/main.cpp`

Itt áll össze a cső. Minden iteráció:

1. a műhold legenerál egy telemetria-mintát (`satellite.sample`),
2. a keretező bekeretezi (`framer.encode`),
3. (opcionálisan) néhány véletlen „vonali zaj" bájtot is a dekóderbe tolunk, hogy
   a SYNC-keresés és resync élesben is dolgozzon,
4. a csatorna elrontja a keretet (`channel.transmit`),
5. a kapott bájtokat **egyesével** a dekóderbe adjuk (`decoder.feed`),
6. a dashboard kirajzolja az állapotot, majd alszunk egy keretnyit.

Alapból a BER egy 32 másodperces ciklusban lépdel tisztától a zajosig és vissza
(`ber_sweep`), így a demó **önjáró**: látod, ahogy a CRC-hibák nőnek, majd a
link tisztulásával helyreáll. A `SIGINT` (Ctrl-C) tisztán kilép és visszaadja a
kurzort.

---

## 10. Tesztek — `tests/test_main.cpp`

A **teszt-létra** alulról fölfelé, mindegyik fok egy konkrét tulajdonságot
bizonyít. Egy mini assert-keret (`CHECK`) számolja a hibákat. Sorrendben:

1. **CRC** — a kanonikus `0x29B1` ellenőrzőérték.
2. **Szerializáció** — körbe-vissza minden mező (a negatívak is), és a big-endian
   bájtsorrend.
3. **Keretező** — jól formált keret, növekvő sorszám.
4. **Dekóder körbe-vissza** — zaj nélkül pontosan ugyanaz jön vissza, és csak az
   *utolsó* bájt ad `true`-t.
5. **SYNC-keresés** — a keret elé szúrt szemét ellenére dekódol.
6. **Két keret egymás után** — mindkettő érvényes, a sorszám nő.
7. **CRC elkapja a payload-bithibát.**
8. **CRC elkapja a CRC-mező bithibáját.**
9. **Resync csonka keret után** — a következő teljes keret mégis dekódol.
10. **Hibás fejléc elutasítása** — resync, a jó keret átmegy.
11. **Sorszám-rés** — a hiányzó keretek eldobottként számolódnak.
12. **Kínzóteszt** — 1 000 000 véletlen bájt: soha nem omlik össze, nem ékelődik
    be, és utána egy valódi keret még mindig dekódol.

Ha mind a 12 zöld, a vevő „repülésbiztos" abban az értelemben, amit a kiírás a
tesztelésnél kér.

---

## 11. Mit nézz, amikor futtatod

```sh
make            # lefordít mindent
make test       # a teljes tesztcsomag (ALL 42 CHECKS PASSED a cél)
make run        # élő dashboard, önjáró link-romlással
make snapshot   # egyetlen állókép (jó képernyőfotóhoz)
```

Próbáld ki a kontrasztot:

```sh
./satlink --once --ber 0      # tiszta link → 100% áteresztés, 0 CRC-hiba
./satlink --once --ber 1e-3   # romlott link → nő a CRC-hiba, esik az áteresztés
./satlink --once --ber 1e-2   # rossz link → sok eldobott keret
```

A **Frame yield** (áteresztés) és a **CRC errors** számláló a bizonyíték: nincs
kétség, hogy működik-e — akkor jó, ha az ép telemetria átmegy, a rontott pedig
elakad a CRC-n.

---

## 12. Beágyazott tanulságok (mehetnek az önéletrajzba)

- Állapotgépes **bájtfolyam-parser** blokkolás és heap nélkül (UART-ISR minta).
- Fix szélességű egészek **big-endian** pakolása/kicsomagolása.
- **CRC-16-CCITT** hibadetektálás, ismert teszt-vektorral igazolva.
- Bejövő stream **görgő szinkronregiszteres** kezelése, folyamatos resynccel.
- **ADC → mérnöki egység** skálázás (skálázott egész), RTC-időbélyeg.
- **Védekező feldolgozás:** sose bízz a bemenetben; bármilyen szemétből vissza
  kell tudni szinkronizálni.
- **Tesztkörnyezet** hibainjektálással és fuzz-teszttel.

---

## 13. Hol kísérletezz

- `protocol.hpp`: adj a payloadhoz új mezőt (pl. `current_ma`). Frissítsd a
  `serialize`/`deserialize` párost, a `payload_len`-t és a dashboardot — jó
  gyakorlat arra, hogy egy protokoll-bővítés mindenhol végigér.
- `satellite.cpp`: változtasd a hullámformákat vagy a periódusokat.
- `decoder.cpp`: próbáld ki, mi történik, ha kiveszed a folyamatos
  SYNC-detektálást (visszaesel a naiv v1-re) — és nézd meg, hogy a 9. teszt
  ekkor másképp viselkedik.
- `main.cpp` `ber_sweep`: írd át a link-romlási menetrendet.
