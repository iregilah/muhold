# CubeSat Telemetria Linkszimulátor — részletes, szakdolgozatszerű leírás

*A `satlink` projekt teljes működése, folyó szövegben, alulról fölfelé felépítve. Ez a dokumentum nem a száraz referencia párja, hanem egy összefüggő történet arról, hogy mit csinál a kód, miért pont úgy, és milyen modern C++ eszközökkel.*

---

## Hogyan olvasd ezt (és miért így van megírva)

Ez egy hosszú anyag, de **nem kell egyben elolvasnod**. Minden fejezet önállóan megáll: van egy nyitó kérdése („mire jó ez egyáltalán?"), egy középső része, ahol soronként végigmegyünk a kódon, és egy záró bekezdése, ami összeszedi, mit nyertél vele. Olvashatsz egy fejezetet egy leüléssel, becsukhatod, és három nap múlva folytathatod a következővel anélkül, hogy elvesztenéd a fonalat.

A felépítés szándékosan **alulról fölfelé** halad. Először a legkisebb, legönállóbb darabokat építjük fel (a protokoll, a bájtok rendezése, a CRC), aztán ezekből rakjuk össze az adóoldalt, a csatornát, majd a projekt szívét, a dekódert, és végül a látványos kimenetet meg a tesztelést. Mire a dekóderhez érsz, már minden fogalom a kezedben van, amit használ — nem kell előrelapoznod.

A nyelvi magyarázatok **a kód mellett, helyben** vannak, nem egy külön szótárban. Amikor egy modern (C++11 utáni) elem először előfordul — egy lambda, egy `enum class`, egy `constexpr`, egy `std::mt19937` —, ott és akkor megmagyarázom, abban a kontextusban, ahol épp dolgozik. Így nem kell fejben tartanod egy hivatkozás-rendszert; az ismeret ott jön, ahol kell.

A fejezetek sorrendje:

1. **Bevezetés** — mi a feladat, miért pont ez a projekt, és hogyan képezi le a C3S beágyazott pozíció elvárásait.
2. **Elméleti háttér** — a fogalmak, amikre az egész épül: telemetria, keretezés, szinkronszó, CRC, bájtsorrend, állapotgép, és a beágyazott megkötések.
3. **A rendszer madártávlatból** — az architektúra, a teljes adatfolyam (a „cső"), a keret bájttérképe és a modulszerkezet.
4. **A protokoll** (`protocol.hpp`) — a közös igazság, amiben az adó és a vevő megegyezik.
5. **Szerializáció és bájtsorrend** (`serialization`) — hogyan lesz a memóriabeli adatból vezetékre tehető bájtsorozat.
6. **Hibadetektálás: a CRC** (`crc16`) — a pecsét, ami elárulja, ha a vezetéken bármi megsérült.
7. **Az adóoldal: a keretező** (`framer`) — a műhold, ami kész keretet gyárt.
8. **A zajos csatorna** (`channel`) — a rádiólink, ami véletlenszerűen biteket billent.
9. **A vevő szíve: a dekóder állapotgép** (`decoder`) — a leghosszabb, legrészletesebb fejezet.
10. **A műhold-szimulátor** (`satellite`) — a szinuszos „élethű" telemetria-forrás.
11. **A látványos kimenet: a dashboard** (`dashboard`) — élő ANSI-kijelző, és egy tanulságos igazítási hiba.
12. **A fő ciklus és a parancssor** (`main`) — ami mindent összeköt.
13. **Tesztelés: a teszt-létra** (`test_main`) és a **Makefile**.
14. **Tudatos döntések** — amit szándékosan *nem* csináltunk, és miért számít ez repülő szoftverben.
15. **Futtatás, bizonyítás, továbblépés.**

A **9. fejezet a mag.** Ha egyetlen részt akarsz tényleg a sajátodnak érezni, az a dekóder. Minden más azért van, hogy a dekódernek legyen mit fogadnia, és hogy lásd is, mit csinál.

---

## 1. Bevezetés — mi ez, és miért épp ez

### A feladat egyetlen mondatban

A `satlink` egy **műhold-földiállomás telemetriai kapcsolat** teljes szimulációja egyetlen laptopon, hardver nélkül: egy szimulált műhold házi-adatokat (akkufeszültség, hőmérsékletek, helyzetszög) **csomagokba keretez**, átküldi egy **zajos rádiócsatornán**, ami véletlenszerűen biteket billent, a vevőoldal pedig **bájtonként, egy állapotgéppel** fejti vissza, **CRC-vel ellenőrzi**, és egy **élő terminál-kijelzőn** mutatja az értékeket meg a hibastatisztikát.

Egyetlen külső függősége sincs: egy C++17-es fordító (`g++`) és a standard könyvtár — semmi más. Ez nem véletlen: pont az a lényeg, hogy bárhol, bármikor lefordítsd és lefuttasd, az elektronikai felszerelésedtől távol is.

### Honnan jött az ötlet

A projekt egy konkrét álláshirdetésre készült: a **C3S** beágyazott szoftverfejlesztő mérnök pozíciójára. A C3S cubesateket és műhold-alrendszereket gyárt (fedélzeti számítógép, energiarendszer, kommunikáció), és a kiírás gerince három dolog köré szerveződik: **hardver-közeli C/C++**, **beágyazott kommunikációs protokollok**, és erős **tesztelhetőség, robusztusság**. A projektet kifejezetten úgy találtuk ki, hogy ezt a hármat egyszerre gyakoroltassa — méghozzá látványosan, hogy egy interjún meg is tudd mutatni, ne csak elmeséld.

### Miért pont ez a projekt készít fel a munkára

Érdemes pontosan látni, mire lősz, mert a projekt szinte minden eleme egy az egyben leképez egy sort a kiírásból.

A **hardver-közeli C/C++ és a kommunikációs protokollok** pontosan az, amit a keretezés gyakoroltat: a szinkronszó, a fejléc, a payload és a CRC összefűzése, majd a bájtszintű visszafejtés. Ez a minta — keret köré szervezett, bájtonként érkező adat — a UART, az SPI, a CAN és az USB közös magja. Ha ezt egyszer rendesen megírtad, az összes többi protokoll csak variáció rá.

A **beágyazott operációs rendszerek** (a kiírásban a FreeRTOS) világához a dekóder visz közel: egy bájtonkénti, nem-blokkoló, dinamikus memóriafoglalás nélküli feldolgozó pontosan az a minta, amit egy UART-megszakításból hívott vevő-rutinban használnál. Ez talán a legfontosabb beágyazott készség, amit ezzel a projekttel demonstrálni tudsz, és a dekóder épp ezért lett a projekt szíve.

A **szoftvertesztelési módszerek** sort maga a hibainjektáló csatorna meg a számlálók fedik le: ezek együtt egy tesztkörnyezetet alkotnak, amiben bizonyítani lehet, hogy a vevő detektálja a hibát és visszaszinkronizál. A kínzóteszt — egymillió véletlen bájt a parserbe, és bizonyítva, hogy nincs összeomlás és nincs beékelődés — szó szerint ennek a sornak a teljesítése.

A kiírás előnyként említi a **Flash/EEPROM/ADC/DAC, RTC** ismeretét; a telemetria-mezők pont ezeket modellezik. A nyers ADC-számból mérnöki egységgé skálázott feszültség, az RTC-ből olvasott időbélyeg — ezek a fogalmak ott vannak a `Telemetry` rekord minden mezőjében.

Végül a **rendszerszemlélet, komplex rendszerek átlátása** maga az egész cső: az adó, a csatorna, a vevő és a kijelző egyetlen, végigkövethető adatfolyamban.

És van egy ráadás ütőkártya. A projekt nem kitalált értékekkel dolgozik, hanem **valódi űrszabványból** vesz át kettőt: a szinkronszó a CCSDS szabvány tényleges „Attached Sync Marker" mintája (`0x1ACFFC1D`), a hibaellenőrzés pedig a CRC-16/CCITT-FALSE. Ettől ez nem egy random gyakorlat, hanem egy szabványra épülő portfólió-darab — egy interjún beszédtéma, nem sorszám.

### Mit fogsz a végére érteni

Ha végigolvasod, úgy fogod ismerni ezt a kódot, mintha te írtad volna: tudni fogod, miért nem szabad egy struktúrát csak úgy a vezetékre másolni; mi az a big-endian és miért az; hogyan működik egy CRC mint polinomos osztás; hogyan épül fel egy állapotgép, ami bármilyen szemétből visszatalál a szinkronba; és közben szépen lassan visszajön a modern C++ is — a `constexpr`, az `enum class`, a lambdák, az `auto`, a `<random>`, a `<chrono>`, és az a tucatnyi apró döntés, ami egy mai C++ kódot megkülönböztet egy 2005-ös C++-tól.

---

## 2. Elméleti háttér — a fogalmak, amikre minden épül

Mielőtt egyetlen sor kódot is megnéznénk, érdemes letenni hét fogalmat. Ez a hét adja a közös szókincset; a kód innentől csak ezeknek a megvalósítása.

### Telemetria

A **telemetria** egyszerűen „távolról mért adat": a műhold házi-mérései, amiket lesugároz a Földre, hogy lássuk, él-e, melegszik-e, mennyi az akku. A mi telemetriánk egyetlen pillanatképe az akkufeszültség, két panel (az energiarendszer és a fedélzeti számítógép) hőmérséklete, egy helyzetszög és egy üzemmód. Ezt a pillanatképet hívjuk egy **mintának** (sample).

Egy fontos beágyazott elv rögtön itt megjelenik: a buszon **nem lebegőpontos számokat** küldesz, hanem kompakt, **skálázott egészeket**. Egy hőmérséklet nem `23.5` (float), hanem `235` — vagyis 0,1 °C-os lépésekben mért egész. A nyers ADC-számból mérnöki egységgé csak akkor alakítasz, amikor kijelzed. Ez sávszélességet spórol, és elkerüli a lebegőpontos formátumok kétértelműségét két különböző hardver között.

### Keretezés (framing)

A vezetéken egy folytonos bájtfolyam érkezik. Honnan tudja a vevő, hol kezdődik egy üzenet és hol végződik? A válasz a **keretezés**: az adatot egy jól definiált burokba csomagoljuk. A mi keretünk négy részből áll, és pont úgy működik, mint egy boríték: van rajta egy feltűnő **bélyeg** (a szinkronszó, amiről a vevő felismeri, hogy „itt kezdődik valami"), egy **fejléc** (ki küldte, hányadik üzenet, milyen hosszú), maga a **levél** (a payload, azaz a telemetria), és egy **viaszpecsét** a végén (a CRC, amiről látszik, ha útközben felbontották vagy megsérült).

### Szinkronszó (sync word)

A **szinkronszó** egy előre megegyezett, feltűnő bájtminta a keret legelején. A vevő a zajban ezt a mintát keresi; amíg nem találja, addig „vadászik". A miénk a CCSDS szabvány 32 bites mintája, `0x1ACFFC1D`. Azért jó egy hosszú, „random kinézetű" minta, mert kicsi az esélye, hogy a valódi adatban véletlenül előforduljon, és így téves keretkezdetet jelezzen.

### CRC — ciklikus redundancia-ellenőrzés

A **CRC** egy rövid ellenőrzőösszeg, amit az adó kiszámol az üzenetből, és mellécsatol. A vevő ugyanazt a számítást elvégzi a kapott adaton, és összeveti. Ha akár egyetlen bit is megsérült útközben, az újraszámolt érték szinte biztosan eltér, és a keret elbukik. A CRC matematikai magja egy **polinomos osztás** egy speciális számtanban (a GF(2) testben, ahol az összeadás az XOR) — erről a 6. fejezetben részletesen lesz szó. Most elég annyi: a CRC nem *javít*, csak *detektál*. Megmondja, hogy a keret romlott, de nem mondja meg, mit kéne tenni — azt a keret eldobjuk és visszaszinkronizálunk.

### Bájtsorrend (endianness) és big-endian

Egy többbájtos szám, mondjuk a 16 bites `3700` (hexában `0x0E74`), kétféleképpen tehető bájtokba: vagy a magas bájt megy előre (`0x0E`, `0x74`), vagy az alacsony (`0x74`, `0x0E`). Az előbbi a **big-endian**, az utóbbi a **little-endian**. A baj az, hogy a CPU-k különbözőek: az x86 (a laptopod) little-endian. Ha két különböző hardver csak úgy egymásnak küldené a memóriabeli bájtjait, nem értenék egymást. A megoldás egy **megegyezett bájtsorrend**, és a hálózati meg az űr-protokollok hagyományosan a big-endiant választják (a legmagasabb helyiérték elöl — pont ahogy mi is balról jobbra írjuk a számokat). A mi protokollunk minden többbájtos mezője big-endian.

### Állapotgép (state machine)

A vevő nem látja egyszerre a teljes keretet — bájtonként kapja, mint egy UART-megszakítás. Tartania kell tehát egy belső **állapotot**, ami megmondja, „épp hol tartok": a szinkronszót keresem, a fejlécet gyűjtöm, a payloadot gyűjtöm, vagy a CRC-t várom. Ahogy érkeznek a bájtok, az állapotgép **átmenetekkel** lép egyik állapotból a másikba. Ez a minta — kevés állapot, bájtonkénti átmenetek, és bármilyen hibára visszaugrás a kiindulóállapotba — a beágyazott protokoll-feldolgozás kenyere.

### A beágyazott megkötések

Két szabály végigvonul az egész vevőoldalon, és ezek teszik a kódot „repülésbiztossá". Az első: **nincs dinamikus memóriafoglalás.** A vevő nem hív `new`-t, nincs `std::vector`, ami nőne; minden puffer fix méretű, előre lefoglalt tagváltozó. Egy beágyazott rendszerben a heap töredezettsége és a kiszámíthatatlan foglalási idő valódi kockázat, ezért a repülő szoftver gyakran egyáltalán nem allokál futás közben. A második: **nincs blokkolás.** A `feed()` egyetlen bájtot dolgoz fel és azonnal visszatér; soha nem vár, nem alszik, nem olvas. Ettől lesz a kód közvetlenül használható egy valódi megszakításkezelőben, ahol minden mikroszekundum számít.

Ezzel megvan a hét fogalom. Innentől a kód már „csak" ezeknek a gondos megvalósítása — és pont a gondosság a tanulság.

---

## 3. A rendszer madártávlatból

### Az adatfolyam: a „cső"

A legjobb mentális kép a `satlink`-ről egy **összeszerelő szalag**, aminek az egyik végén beömlik egy telemetria-minta, a másik végén pedig kijön a képernyőre rajzolt érték. Öt állomás van a szalagon:

```
  ┌───────────┐    ┌─────────┐    ┌──────────────┐    ┌───────────┐    ┌─────────────┐
  │ Satellite │ →  │ Framer  │ →  │ NoisyChannel │ →  │  Decoder  │ →  │  Dashboard  │
  │ (érzékelő)│    │(boríték)│    │ (zajos rádió)│    │(állapotgép)│    │ (kijelző)   │
  └───────────┘    └─────────┘    └──────────────┘    └───────────┘    └─────────────┘
   Telemetry        kész keret      megbillent          visszafejtett     élő képernyő
   rekord           (23 bájt)       bájtok              Telemetry +
                                                        statisztika
```

Balról jobbra: a **Satellite** legenerál egy `Telemetry` mintát (mintha leolvasná az érzékelőket). A **Framer** ezt borítékba teszi: kész 23 bájtos keretet gyárt belőle. A **NoisyChannel** átengedi a keretet egy zajos rádión, ami adott valószínűséggel biteket billent. A **Decoder** a megérkezett (és talán megsérült) bájtokat egyesével fogadja, és megpróbálja visszafejteni — ha sikerül és a CRC stimmel, kiad egy `Telemetry`-t; ha nem, eldobja és visszaszinkronizál. A **Dashboard** pedig az utolsó sikeres mintát meg a futó statisztikát rajzolja a képernyőre.

Ez pontosan az az út, amit egy földi állomás szoftvere bejár egy valódi rádiólink végén — csak nálunk a rádió is, a műhold is szoftverből van. Az `src/main.cpp` fő ciklusa szó szerint ezt az öt lépést hajtja végre minden iterációban.

### A keret bájttérképe

Az egész projekt egyetlen megállapodáson nyugszik: hogyan néz ki egy keret a vezetéken. Ez 23 bájt, és így épül fel (minden többbájtos mező big-endian):

```
 Offszet  Méret  Mező           Érték / jelentés              CRC fedi?
 -------  -----  -------------  ----------------------------  ---------
   0       4     SYNC           0x1A 0xCF 0xFC 0x1D           NEM
   4       1     version_type   0x01                          igen
   5       2     seq_count      keretenként nő                igen
   7       1     payload_len    13 (a Telemetry mérete)       igen
   8      13     payload        szerializált Telemetry        igen
  21       2     crc16          CRC-16/CCITT-FALSE            — (ez maga a CRC)
```

Két részletet érdemes most rögtön kiemelni, mert ezeken áll vagy bukik az egész. Az első: a **CRC a 4..20-as bájtokat fedi**, vagyis a fejlécet és a payloadot együtt, 17 bájtot. A szinkronszó **nincs** benne (az csak jelző, nem adat), és maga a CRC-mező sincs benne (azt épp most számoljuk). Ha ezt a tartományt elvéted — mondjuk beleszámolod a SYNC-et —, akkor *minden* keret elbukik a CRC-n, és órákig fogod keresni, mi a baj. A második: a payload 13 bájtja egy `Telemetry` rekord, mezőnként, pontosan rögzített sorrendben szerializálva.

### A modulszerkezet

A projekt nem egyetlen nagy fájl, hanem tiszta, moduláris felépítésű: minden réteg külön `.hpp`/`.cpp` párt kap. A fejléc (`include/`) mondja meg, *mit* tud a modul; a forrás (`src/`) mondja meg, *hogyan*. Ez a szétválasztás nemcsak rendezett, hanem gyakorlati haszna is van: a `Makefile` fejléc-függőséget követ, így egy fejléc módosítása pontosan a megfelelő fájlokat fordítja újra.

A rétegek, alulról fölfelé:

| Réteg | Fájl | Mit csinál |
|---|---|---|
| Protokoll | `protocol.hpp` | a keretformátum, a `Telemetry`, a konstansok |
| Szerializáció | `serialization.cpp` | big-endian be- és kicsomagolás |
| CRC | `crc16.cpp` | CRC-16/CCITT-FALSE |
| Keretező | `framer.cpp` | `Telemetry` → kész keret (adóoldal) |
| Csatorna | `channel.cpp` | bithiba-injektálás |
| Dekóder | `decoder.cpp` | bájtfolyam-állapotgép — a lényeg |
| Műhold | `satellite.cpp` | szinuszos telemetria-forrás |
| Dashboard | `dashboard.cpp` | élő ANSI-kijelző |
| Fő ciklus | `main.cpp` | a teljes cső + parancssor |
| Tesztek | `test_main.cpp` | a teszt-létra |

Az egész egyetlen **névtérben** él, a `satlink`-ben. Erről mindjárt, a következő fejezetben szót ejtünk — ahogy az első igazi nyelvi elemekről is, amikkel a `protocol.hpp` tele van.

A következő fejezetektől belemerülünk a kódba. A sorrend a szalag sorrendje: protokoll, szerializáció, CRC, adóoldal, csatorna, dekóder, műhold, kijelző, fő ciklus, tesztek.

---

## 4. A protokoll — a közös igazság (`protocol.hpp`)

Minden kommunikáció egy megállapodáson áll: az adó és a vevő pontosan ugyanúgy kell, hogy értse a bájtokat. A `protocol.hpp` ennek a megállapodásnak a leírása — *az egyetlen igazságforrás*, amire az egész projekt hivatkozik. Ha itt elrontasz egy konstanst, az hibája végigfut a kódon. Épp ezért ez a fájl szinte csak deklarációkból áll: nincs benne logika, csak nevek és értékek. Viszont pont ezek a nevek hordozzák az első adag modern C++-t, úgyhogy lassítsunk le rajtuk.

### A fájl tetje: `#pragma once` és a fix szélességű típusok

A fájl elején ezt látod:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
```

A `#pragma once` a **modern include-őr**. A régi C++-ban erre a `#ifndef PROTOCOL_HPP / #define PROTOCOL_HPP / ... / #endif` hármast használtad, hogy egy fejléc kétszeri beemelése ne okozzon dupla-definíciós hibát. A `#pragma once` ugyanezt egy sorban elintézi, elgépelhetetlen makrónevek nélkül. Szigorúan véve nem szabványos, de minden mai fordító (gcc, clang, MSVC) ismeri, és gyakorlatilag mindenhol ezt használják.

A két include a **fix szélességű egész típusokat** hozza be. A `<cstdint>` adja a `uint8_t`, `uint16_t`, `uint32_t`, `int16_t` típusokat, a `<cstddef>` pedig a `size_t`-et. Ez nem kozmetika: egy protokollnál *életbevágó*, hogy egy mező pontosan 16 bit legyen, ne „legalább 16, de a fordítótól függ". A sima `int` mérete platformfüggő; a `uint16_t` mindig, mindenhol pontosan 2 bájt. Amikor a vezetékre dolgozol, a típusneved egyben a méret garanciája is.

### Névtér: `namespace satlink`

Az egész projekt egy `namespace satlink { ... }` blokkban él. Ezt te ismered a régi C++-ból is; a lényeg, hogy minden név (a `Telemetry`, a `Decoder`, a `crc16_ccitt`) a `satlink::` előtag alá kerül, így soha nem ütközik egy másik könyvtár azonos nevű dolgaival. A `.cpp` fájlok a `using namespace satlink;` sorral hozzák be a teljes névteret, hogy ne kelljen mindenhova kiírni az előtagot.

### A konstansok: `constexpr` a `#define` helyett

Itt jön az első tényleg fontos modern fordulat. A régi C++-ban egy fordítási idejű konstanst vagy `#define`-nal csináltál (`#define SYNC 0x1ACFFC1D`), vagy `const`-tal. Ma erre a `constexpr` van:

```cpp
constexpr uint32_t kSyncWord = 0x1ACFFC1D;
constexpr uint8_t  kVersionType = 0x01;

constexpr size_t kSyncSize    = 4;
constexpr size_t kHeaderSize  = 4;
constexpr size_t kPayloadSize = 13;
constexpr size_t kCrcSize     = 2;
```

Miért jobb ez a `#define`-nál? Mert a `#define` egy buta szövegcsere, amit a fordító még a tényleges fordítás előtt elvégez: nincs típusa, nincs hatóköre, és ha hibázol, a hibaüzenet a behelyettesített szövegről szól, nem a névről. A `constexpr` ezzel szemben egy *valódi, típusos* konstans, ami **fordítási időben** ki van számolva (tehát futásidőben nem kerül semmibe), de közben rendes C++ entitás: van típusa (`uint32_t`, `size_t`), beleesik a `satlink` névtérbe, és a fordító ellenőrzi. A `kSyncWord` egy igazi `uint32_t`, nem egy szövegdarab.

A `constexpr` ereje akkor mutatkozik meg igazán, hogy **számolni is tudsz** vele fordítási időben:

```cpp
constexpr size_t kFrameSize = kSyncSize + kHeaderSize + kPayloadSize + kCrcSize; // 23
constexpr size_t kCrcCoveredSize = kHeaderSize + kPayloadSize; // 17
```

A `kFrameSize` nem egy bemásolt `23`-as, hanem a részekből *kiszámolt* érték. Ha holnap nő a payload, a keret mérete magától követi — egy helyen kell módosítani, nem ötben. A `kCrcCoveredSize` pedig azért kap saját nevet, mert — ahogy a fájl kommentje is figyelmeztet — ez a leggyakoribb hely, ahol el lehet rontani a protokollt. Ha a CRC-tartomány egy nevesített konstans, akkor egy helyen, feketén-fehéren ott áll, hogy 17 bájt, és nincs min vitatkozni.

A névadásról egy szó: a `k` előtag (`kSyncWord`, `kPayloadSize`) a Google C++ stílusból jön, és azt jelöli, hogy „ez egy fordítási idejű konstans". Pusztán konvenció, de következetes, és ránézésre megkülönbözteti a konstansokat a változóktól.

### A védekező felső korlát

```cpp
constexpr size_t kMaxPayloadSize = 255;
```

Ez a konstans elsőre fölöslegesnek tűnik — hiszen a payload mindig 13 bájt. De a szerepe nem a normál működés, hanem a **védekezés**. A vevő fix méretű puffereket használ, és ezeket a *legrosszabb esetre* méretezi előre. A payload hosszát a keret fejléce mondja meg (`payload_len`), de a vevő **soha nem bízik meg** ebben az értékben annyira, hogy belőle foglaljon helyet. Ehelyett már induláskor lefoglal annyit, amennyi a maximum (255 bájt), és bármit elutasít, ami ennél nagyobbat állítana. Hogy ez miért létkérdés: ha egy elrontott fejléc egy óriási hosszt hazudna, és te abból méreteznél puffert vagy abba másolnál, az klasszikus **puffer-túlcsordulás** — pontosan az a fajta hiba, ami repülő szoftverben katasztrófa. A 255-ös korlát az a fal, amin a hazugság megáll.

### Az üzemmód: `enum class Mode : uint8_t`

```cpp
enum class Mode : uint8_t {
    Safe    = 0,  // minimális fogyasztás, hibajavítás
    Nominal = 1,  // normál házi-üzem
    Payload = 2,  // tudományos műszer aktív
    Comms   = 3,  // nagy teljesítményű letöltés
};
```

A műhold üzemmódja egyetlen bájt a vezetéken, de a kódban nem nyers szám, hanem egy **scoped enum**, vagyis `enum class`. Ez három okból jobb a régi C-stílusú `enum`-nál.

Először is **típusbiztos**: egy `Mode` nem keveredik véletlenül egy `int`-tel. A régi `enum` értékei csak úgy átfolytak egészekké, így könnyű volt értelmetlen összehasonlításokat vagy aritmetikát írni velük. Az `enum class` ezt megtiltja; ha `int`-et akarsz belőle, azt ki kell mondanod egy `static_cast`-tal — pontosan ezt teszi a szerializáció, amikor a bájtra teszi.

Másodszor **nem szennyezi a névteret**: a `Safe`, `Nominal` stb. nevek a `Mode::` alá vannak zárva (`Mode::Safe`), nem ömlenek ki a környező hatókörbe. A régi `enum`-nál egy szabad `Safe` név bárhol ütközhetett volna.

Harmadszor — és ez beágyazott szempontból kulcs — a `: uint8_t` **rögzíti a tárolási méretet**. Megmondjuk a fordítónak, hogy ezt az enumot pontosan egy bájton tárolja. Ettől a `Mode` mérete kiszámítható és illeszkedik a protokollhoz: egy bájt a vezetéken, egy bájt a memóriában.

### A telemetria-rekord és az alapértelmezett tagértékek

```cpp
struct Telemetry {
    uint32_t timestamp_ms  = 0;            // ms a bekapcsolás óta, RTC-ből
    uint16_t batt_mv       = 0;            // akkufeszültség, millivolt
    int16_t  temp_eps_c10  = 0;            // EPS-panel hőmérséklet, 0.1 °C lépés
    int16_t  temp_obc_c10  = 0;            // OBC-panel hőmérséklet, 0.1 °C lépés
    int16_t  attitude_cdeg = 0;            // helyzetszög, 0.01° lépés (±327°)
    Mode     mode          = Mode::Safe;   // üzemmód
};
```

Ez a rekord a házi-adatok egyetlen pillanatképe, és minden részlete tudatos. A mezők **fix szélességű egészek**, mert a vezetékre kerülnek. A nevekbe bele van kódolva a **skálázás**: a `temp_eps_c10` neve azt üzeni, hogy az érték °C *tízszerese* (a `c10` = „centi-tíz", azaz 0,1 °C-os lépés), tehát a `235` valójában 23,5 °C. Hasonlóan az `attitude_cdeg` 0,01°-os lépésekben tárol (a `cdeg` = centi-fok), így a `−4500` valójában −45,00°. Ezeket a skálákat szándékosan úgy választották, hogy a teljes várható tartomány még beleférjen egy előjeles 16 bites mezőbe: a `int16_t` ±32767-ig megy, ami 0,01°-os lépésnél ±327°-ot jelent — bőven elég a ±180°-os helyzetszöghöz.

A modern elem itt az `= 0`, illetve az `= Mode::Safe` a mezők után. Ezek az **alapértelmezett tagértékek** (default member initializers), egy C++11-es újdonság. A régi C++-ban egy `struct`-ot vagy kézzel kellett nullázni, vagy egy konstruktorban inicializálni; ha elfelejtetted, a mező *szemét* értékkel jött létre. Itt minden mező magától nulláról (illetve `Safe`-ről) indul, amint létrejön egy `Telemetry`. Egy `Telemetry t;` rögtön egy tiszta, nullázott rekord — nincs véletlen szemét, ami később megmagyarázhatatlan hibát okozna.

Ennyi a protokoll. Egyetlen sornyi „működés" sincs benne, mégis ez a fájl tartja össze az egészet: az itt definiált nevekre és értékekre hivatkozik minden további modul. A következő kérdés, hogy ebből a szép, memóriabeli `Telemetry`-ből hogyan lesz vezetékre tehető bájtsorozat — ez a szerializáció dolga.

---

## 5. Szerializáció és bájtsorrend (`serialization.hpp` / `serialization.cpp`)

Itt jön az egyik legtanulságosabb beágyazott lecke, és érdemes a *miértnél* kezdeni, mert ez az a hiba, amit szinte mindenki egyszer elkövet.

### Miért nem szabad egy struktúrát a vezetékre másolni

Kísértés volna a `Telemetry`-t egyszerűen lemásolni a kimenő pufferbe egy `memcpy`-vel — hiszen ott van készen a memóriában. **Ne tedd.** Két okból is rossz.

Az első a **padding** (kitöltés). A fordító szabadon beszúrhat üres bájtokat a struktúra mezői közé, hogy igazítva legyenek a memóriában (mert a CPU gyorsabban olvas igazított adatot). A `Telemetry` memóriabeli mérete tehát lehet nagyobb, mint a mezők összege, és a kitöltőbájtok helye fordítófüggő. Ha ezt másolod a vezetékre, ismeretlen szemetet is küldesz a hasznos adat közé.

A második a **bájtsorrend**, amiről a 2. fejezetben már volt szó. A memóriabeli bájtsorrend a CPU-tól függ; az x86 little-endian. Ha a laptopod little-endian bájtjait küldöd egy big-endian rendszernek, az fordítva olvassa a számokat.

A megoldás mindkettőre ugyanaz: **ne másolj, hanem szerializálj mezőnként**, explicit, megegyezett bájtsorrenddel. Ez pár sorral több munka, de cserébe a vezetéken lévő formátum teljesen független a fordítótól és a CPU-tól. A két oldal (adó és vevő) ugyanazt a bájtkiosztást használja, és kész.

### Az alapelemek: big-endian put és get

A `serialization.hpp` hat apró függvényt deklarál, amik egy-egy primitív értéket írnak vagy olvasnak a megadott bájtsorrendben. Mind **mutatóval** dolgozik: a hívó adja a célcímet, és ő felel azért, hogy legyen elég hely. A függvények soha nem írnak/olvasnak a saját szélességükön túl.

```cpp
void put_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);  // magas bájt előre
    p[1] = static_cast<uint8_t>(v & 0xFF);         // alacsony bájt
}

void put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}
```

A logika a big-endian definíciója: a legmagasabb helyiértékű bájt a legalacsonyabb címre. A `v >> 8` lehozza a magas bájtot az alsó 8 bitbe, a `& 0xFF` kimaszkolja, és a `static_cast<uint8_t>` letárolja. A bitmanipuláció maga ismerős a régi C++-ból; két modern részlet van itt, amin érdemes megállni.

Az egyik a `static_cast<uint8_t>`. A régi C++-ban ezt `(uint8_t)`-ként írtad volna. A modern C++ a **nevesített castokat** részesíti előnyben — `static_cast`, `reinterpret_cast`, `const_cast`, `dynamic_cast` — a sima C-cast helyett, három okból: kereshetők (rákereshetsz a kódban, hol konvertálsz), kifejezik a *szándékot* (a `static_cast` egy „normál, biztonságos" konverzió), és a fordító szűkebben ellenőrzi őket, mint a mindenható C-castot. Ebben a projektben végig nevesített castokat látsz; ez nem véletlen, hanem stílus. Ráadásul a `Makefile` `-Wall -Wextra -Wpedantic` kapcsolókkal fordít, vagyis minden figyelmeztetés be van kapcsolva — és egy `int`-ből `uint8_t`-be való szűkítés figyelmeztetést váltana ki, amit a `static_cast` elnémít (mert kimondod: tudom, mit csinálok).

A másik a `& 0xFF` a cast után. Szigorúan véve fölösleges: a `static_cast<uint8_t>` úgyis levágja az alsó 8 bitet. De a kommentként szolgál — *dokumentálja a szándékot*, hogy „itt egy bájtot akarok". Olvashatóság a tömörség előtt.

### Az olvasó oldal és egy klasszikus csapda: az integer promotion

```cpp
uint16_t get_u16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                 static_cast<uint16_t>(p[1]));
}
```

Ez a függvény visszarakja a két bájtot egyetlen 16 bites számmá, és itt egy igazi C++-csapda rejtőzik, amit egy egyetemi félév után könnyű elfelejteni: az **integer promotion**. A `p[0]` egy `uint8_t`, de C++-ban *bármilyen, az `int`-nél keskenyebb egészet a fordító automatikusan `int`-té léptet elő*, mielőtt aritmetikát végezne rajta. Tehát a `p[0] << 8` valójában egy `int` eltolás, nem egy `uint8_t` eltolás — és ez itt épp jó, mert a 8 bites érték `int`-ben simán elfér 16 bitre tolva. A kód mégis kiírja a `static_cast<uint16_t>(p[0])`-t, hogy a szándék egyértelmű legyen, és a végén az egészet visszacastolja `uint16_t`-re, mert a `|` eredménye `int`, amit vissza kell szűkíteni a deklarált visszatérési típusra. Ez a fajta explicit castolgatás elsőre soknak tűnik, de pont az integer promotion miatt van: a C++ a háttérben `int`-ekkel számol, és te ezt teszed láthatóvá és helyessé.

### Az előjeles mezők: a kettes komplemens trükkje

```cpp
void put_i16(uint8_t* p, int16_t v) {
    put_u16(p, static_cast<uint16_t>(v));
}

int16_t get_i16(const uint8_t* p) {
    return static_cast<int16_t>(get_u16(p));
}
```

A hőmérséklet és a helyzetszög előjeles lehet (negatív hőmérséklet, negatív szög). Hogyan teszel előjeles számot bájtokba? A válasz gyönyörűen egyszerű: **sehogy, máshogy**. A kettes komplemens ábrázolásban egy előjeles 16 bites szám *pontosan ugyanaz a bitminta*, mint az előjel nélküli párja. A `−1` mint `int16_t` ugyanaz a 16 bit (`0xFFFF`), mint a `65535` mint `uint16_t`. Ezért a `put_i16` egyszerűen `uint16_t`-vé castolja az értéket, és átadja a meglévő `put_u16`-nak; a `get_i16` pedig a visszaolvasott `uint16_t`-t castolja vissza `int16_t`-re. A bitek soha nem változnak; csak az értelmezésük. (A kommentben szerepel, hogy ez a visszacast minden valódi fordítón a helyes értéket adja, és C++20-tól ez már garantált is — korábban elvileg „implementáció-függő" volt, gyakorlatilag mindig így működött.)

Ezt a tesztcsomag külön ellenőrzi: a `put_i16(p, -1)` után `p[0]==0xFF && p[1]==0xFF`, és a `get_i16(p)==-1`. Ez bizonyítja, hogy a negatív értékek is sértetlenül átmennek a vezetéken — pont az a hiba ez, ami egy hanyag megvalósításban elromlana.

### A teljes rekord szerializálása

A primitívekre épül a két fő függvény, ami az egész `Telemetry`-t kezeli:

```cpp
size_t serialize_telemetry(const Telemetry& t, uint8_t* out) {
    put_u32(&out[0],  t.timestamp_ms);
    put_u16(&out[4],  t.batt_mv);
    put_i16(&out[6],  t.temp_eps_c10);
    put_i16(&out[8],  t.temp_obc_c10);
    put_i16(&out[10], t.attitude_cdeg);
    out[12] = static_cast<uint8_t>(t.mode);
    return kPayloadSize;
}

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

A bájtkiosztás kézzel, offszetenként van leírva: a `timestamp_ms` a 0..3, a `batt_mv` a 4..5, a hőmérsékletek a 6..9, a helyzetszög a 10..11, az üzemmód a 12-es bájt — összesen 13, ahogy a `kPayloadSize` ígéri. Az üzemmódnál látszik, miért volt jó az `enum class`: a `static_cast<uint8_t>(t.mode)` és visszafelé a `static_cast<Mode>(in[12])` *kimondatja veled* a konverziót enum és bájt között. Egy régi `enum`-nál ez némán megtörtént volna; itt a fordító megköveteli, hogy láthatóvá tedd — ami pont jó, mert a vezetékre/vezetékről átlépés mindig egy hely, ahol résen kell lenni.

Egy fontos fegyelmi szabály a kódban: a `serialize_telemetry` és a `deserialize_telemetry` **bájtkiosztásának pontosan tükröznie kell egymást**, és a kommentek külön kérik, hogy a két függvényt egymás mellett tartsd és együtt szerkeszd. Ha az egyikben átrendezed a mezőket, de a másikban nem, az adat csendben összekeveredik — a CRC ezt nem fogja elkapni, mert mindkét oldal „helyes" a maga szemszögéből. Ez a fajta tükör-páros hiba a szerializációs kód örök veszélye, és a védelem ellene egyszerű: a két függvény legyen szomszéd, és mindig együtt mozogjon.

Ezzel megvan az út a `Telemetry` és a 13 bájtos payload között, mindkét irányban, fordítótól és CPU-tól függetlenül. A következő réteg a pecsét, ami elárulja, ha a vezetéken bármi megsérült: a CRC.

---

## 6. Hibadetektálás: a CRC (`crc16.hpp` / `crc16.cpp`)

A vezetéken bármi megtörténhet egy bittel: a zaj megbillentheti. A CRC az a mechanizmus, amitől a vevő *észreveszi*, ha ez megtörtént. Ez a projekt egyik legszebb darabja, mert mögötte egy elegáns matematika áll, a kód mégis alig harminc sor.

### Mi az a CRC, és miért működik

A CRC (ciklikus redundancia-ellenőrzés) gondolata egyszerű: az adó az üzenetből kiszámol egy rövid „ujjlenyomatot", és mellécsatolja. A vevő ugyanezt a számítást elvégzi a *kapott* adaton, és összeveti a kapott ujjlenyomattal. Ha akár egyetlen bit megsérült, az újraszámolt érték szinte biztosan eltér.

Hogy *miért* szinte biztos, ahhoz egy pillanatra matematikára vált a dolog — de megéri, mert ettől lesz „a sajátod" a CRC, nem egy varázsdoboz. A trükk: tekintsd az üzenet bitjeit egy hatalmas **bináris polinom** együtthatóinak. Ezt a polinomot elosztod egy rögzített **generátorpolinommal**, és a CRC az **osztási maradék**. A számtan, amiben ez zajlik, a GF(2) test: itt nincsenek átvitelek, és az **összeadás is, a kivonás is ugyanaz a művelet — az XOR**. Az egész CRC nem más, mint egy polinomos **maradékos osztás** ebben a furcsa számtanban.

Ha valaha csináltál kézzel írásbeli osztást, a CRC pont az: nézed a legfelső számjegyet, és ha az osztó „belefér", levonod (itt: XOR-olod) az osztót, lépsz egyet, és ismétled. A végén ott marad a maradék. A különbség csak annyi, hogy itt minden bit, és a kivonás XOR. Ez a felismerés teszi a kódot átláthatóvá.

A miértre a válasz: a polinomos osztás úgy van megválasztva, hogy a tipikus átviteli hibák garantáltan megváltoztassák a maradékot. Egy jó CRC-16 elkap minden 1 és 2 bites hibát, minden páratlan számú bithibát, és minden olyan hibafürtöt, ami legfeljebb 16 bit hosszú — egy 16 bites maradéknál pedig egy véletlen, általános romlás is csak nagyjából 1/65536 eséllyel csúszik át. Pont ezért tűnt a romlott keret a dashboardon mindig a CRC-n elakadni.

### A konkrét paraméterek

Egy CRC-t néhány paraméter határoz meg pontosan, és a miénk a szabványos „CRC-16/CCITT-FALSE" néven ismert változat:

```
polinom:        0x1021   (x^16 + x^12 + x^5 + 1)
kezdőérték:     0xFFFF
bit-tükrözés:   nincs (sem bemeneten, sem kimeneten)
záró XOR:       nincs
```

(A „FALSE" a névben egy történelmi félreértés öröksége — sokáig keverték a valódi CCITT változattal; a lényeg, hogy ez a `0xFFFF` kezdőértékű, nem-tükrözött variáns.) Ennek a variánsnak van egy híres **ellenőrzőértéke**: a `"123456789"` ASCII-szöveg CRC-je pontosan `0x29B1`. Ezt a tesztcsomag külön ellenőrzi, és pont azért jó, mert bármelyik online CRC-kalkulátorral összevetheted — ha a te kódod is `0x29B1`-et ad, akkor a megvalósításod szabványkövető.

### A kód, soronként

```cpp
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    constexpr uint16_t kPoly = 0x1021;
    uint16_t crc = 0xFFFF;  // kezdőérték

    for (size_t i = 0; i < len; ++i) {
        // A következő bájtot a 16 bites regiszter tetejére hozzuk.
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        // A 8 bitet a legmagasabbtól lefelé dolgozzuk fel.
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

Olvassuk együtt. A `crc` regiszter `0xFFFF`-ről indul (ez a kezdőérték). Minden bemeneti bájtra először a `crc ^= data[i] << 8` sor a bájtot a regiszter *felső* 8 bitjébe XOR-olja be — ez hozza „játékba" a következő bájtot. Aztán a belső ciklus nyolcszor lép, mindig a legmagasabb bittel kezdve: megnézi, hogy a regiszter tetején lévő bit (`crc & 0x8000`) be van-e állítva. Ha igen, az azt jelenti, hogy az osztó „belefér", tehát balra told eggyel *és* XOR-olja a polinommal; ha nem, csak balra tol. Ez a két ág pontosan az írásbeli osztás két esete: vagy levonod az osztót, vagy nem. A ciklusok végén a regiszterben maradt érték a CRC.

A modern részlet itt megint az **integer promotion** kezelése. A `crc << 1` művelet előtt a `crc` (egy `uint16_t`) `int`-té léptetődik elő, így az eltolás eredménye `int` — ha a 15. bit kitolódott, az most az `int` 16. bitjében ülne, nem veszne el. Ezért a `static_cast<uint16_t>` *kötelező*: visszaszűkíti az eredményt 16 bitre, levágva a kicsorduló bitet. Ez nem kozmetika; nélküle a CRC rosszul számolódna, és a `-Wpedantic` fordító is panaszkodna a néma szűkítésre. A `kPoly` pedig `constexpr` lokális konstans — ugyanaz az elv, mint a `protocol.hpp`-ben, csak itt egy függvényen belül.

A tesztcsomag a már említett `0x29B1`-en kívül azt is ellenőrzi, hogy **üres bemenetre** (`len == 0`) a CRC a kezdőértékét, `0xFFFF`-et adja — hiszen ha nincs bájt, a belső ciklus le sem fut, és a regiszter érintetlen marad. Apró, de szép határeset.

Ezzel megvan a pecsét. A `framer` ezt fogja rátenni a keretre az adóoldalon, a `decoder` pedig ezt fogja újraszámolni a vevőoldalon. Lássuk előbb az adót.

---

## 7. Az adóoldal: a keretező (`framer.hpp` / `framer.cpp`)

A `Framer` a műhold „adó" oldala: fog egy `Telemetry` rekordot, és kész, vezetékre tehető keretet gyárt belőle — szinkronszó, fejléc, payload, CRC. Egy valódi cubesaten ez az a fedélzeti kód, ami átad egy bájtpuffert a rádió UART-jának.

### Az osztály és a sorszámláló

```cpp
class Framer {
public:
    size_t encode(const Telemetry& t, uint8_t* out, size_t out_cap);
    uint16_t next_seq() const { return seq_; }
private:
    uint16_t seq_ = 0;  // keretenként nő, 65535 után visszafordul 0-ra
};
```

A `Framer` egyetlen állapota a `seq_` sorszámláló, ami minden legyártott keretnél eggyel nő. Erre a vevőnek lesz szüksége: ha a sorszámok között lyuk keletkezik, abból tudja, hány keret veszett el a linken. A `next_seq()` egy **konstans tagfüggvény** (a `const` a végén megígéri, hogy nem módosítja az objektumot), és rögtön a fejlécben, egysorosként van megírva — ezt a fordító beágyazhatja (inline). A `seq_` itt is alapértelmezett tagértékkel (`= 0`) indul, ahogy a `Telemetry` mezői.

### A bufferbe írás filozófiája

A `encode` aláírása szándékosan **nem** ad vissza egy `std::vector`-t. Ehelyett egy hívó által adott pufferbe (`out`) ír, és megkapja annak kapacitását (`out_cap`) is:

```cpp
size_t Framer::encode(const Telemetry& t, uint8_t* out, size_t out_cap) {
    if (out_cap < kFrameSize) {
        return 0;  // nem írunk a hívó pufferén túl
    }
    ...
```

Ez a „a hívó adja a puffert" minta a beágyazott TX-út valósága: nincs heap-allokáció, a memória a híváson kívül van lefoglalva (akár a stacken, akár egy statikus pufferként), és a függvény csak feltölti. Az első sor pedig **védekezés**: ha a megadott puffer kisebb, mint egy keret, a függvény visszaad 0-t, és egy bájtot sem ír — soha nem csordul túl a hívó pufferén. A visszatérési érték a ténylegesen kiírt bájtok száma, ami sikeres esetben `kFrameSize`.

### A keret összerakása, lépésről lépésre

```cpp
    size_t i = 0;

    // 1) SYNC szó. A CRC NEM fedi; csak a keret kezdetét jelzi.
    put_u32(&out[i], kSyncWord);
    i += kSyncSize;

    // 2) Fejléc. Megjegyezzük, hol kezdődik, mert a CRC innen számolódik.
    const size_t crc_region_start = i;
    out[i++] = kVersionType;
    put_u16(&out[i], seq_);
    i += 2;
    out[i++] = static_cast<uint8_t>(kPayloadSize);

    // 3) Payload: a szerializált telemetria.
    serialize_telemetry(t, &out[i]);
    i += kPayloadSize;

    // 4) CRC a fejléc + payload felett, big-endian.
    const uint16_t crc = crc16_ccitt(&out[crc_region_start], kCrcCoveredSize);
    put_u16(&out[i], crc);
    i += kCrcSize;

    ++seq_;  // léptetés a következő kerethez; 16 biten magától visszafordul
    return i;
```

Az `i` egy futó írási index. Először a `put_u32` beírja a 4 bájtos szinkronszót. Aztán — és ez a **legkritikusabb sor az egész fájlban** — egy `crc_region_start` változó megjegyzi, hol kezdődik a fejléc. Erre azért van szükség, mert a CRC nem a keret elejétől, hanem *innen*, a SYNC után számolódik. A fejléc három mezője bekerül: a `version_type` (egy bájt), a `seq_` sorszám (két bájt, `put_u16`-tal big-endianban), és a `payload_len` (egy bájt, a `kPayloadSize` értéke). Ezután a `serialize_telemetry` rárakja a 13 bájtos payloadot. Végül a `crc16_ccitt` kiszámolja a CRC-t — pontosan a `crc_region_start`-tól, `kCrcCoveredSize` (17) bájton —, és a `put_u16` big-endianban a keret végére teszi.

A `++seq_` a legvégén lépteti a sorszámlálót a következő kerethez. Itt egy szép apróság: a `uint16_t` túlcsordulása **jól definiált** — előjel nélküli típusnál a `65535 + 1` magától `0`-ra fordul vissza, nincs sem hiba, sem véletlenszerűség. Tehát a sorszám szépen körbejár, és a vevő ezt a körbejárást is helyesen tudja majd kezelni a lyuk-detektálásnál.

Vedd észre, mennyire végigvonul a protokoll-konstansokra való hivatkozás: `kSyncWord`, `kSyncSize`, `kVersionType`, `kPayloadSize`, `kCrcCoveredSize`, `kCrcSize`. Sehol egy „mágikus szám"; ha a protokoll változik, a `framer` magától követi. Ezt teszteli a tesztcsomag is: ellenőrzi, hogy a keret eleje tényleg `0x1A 0xCF 0xFC 0x1D`, hogy a 4-es bájt a `version_type`, a 7-es a `payload_len`, és hogy a `next_seq()` minden `encode` után eggyel nő.

A keret tehát kész és makulátlan. Most átengedjük egy zajos rádión, hogy legyen mit a vevőnek visszafejtenie — és hibásan visszafejtenie.

---

## 8. A zajos csatorna (`channel.hpp` / `channel.cpp`)

A `NoisyChannel` a rádiólinket modellezi a műhold és a földi állomás között. Minden bit, ami áthalad rajta, adott valószínűséggel — a **bithiba-rátával** (bit error rate, BER) — megbillen. Ez pontosan az a **hibainjektáló mechanizmus**, amire egy tesztkörnyezetnek szüksége van: ezzel lehet bizonyítani, hogy a vevő detektálja a romlást és visszaszinkronizál — mindezt valódi hardver és rádió nélkül.

### A konstruktor és a véletlenszám-generálás

```cpp
class NoisyChannel {
public:
    NoisyChannel(double bit_error_rate, uint32_t seed);
    void transmit(uint8_t* data, size_t len);
    ...
private:
    double                                 ber_;
    std::mt19937                           rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
    uint64_t                               total_bits_   = 0;
    uint64_t                               flipped_bits_ = 0;
};
```

Itt jön be a projekt egyik legmodernebb darabja: a `<random>` fejléc. A régi C++-ban véletlenszámhoz a `rand()`-ot használtad — ami gyenge minőségű, platformfüggő, és rövid a periódusa. A modern C++ ezt két különálló fogalomra bontja, és ez a szétválasztás a lényeg: van egy **motor** (engine), ami nyers véletlen biteket gyárt, és van egy **eloszlás** (distribution), ami ezeket a kívánt alakúvá formálja.

A motor itt a `std::mt19937`: a Mersenne Twister, egy kiváló minőségű, hordozható, **determinisztikus** ál-véletlen generátor, hatalmas periódussal. A neve a 2^19937−1 periódusra utal. Determinisztikus, vagyis ha ugyanazzal a maggal (seed) indítod, *pontosan ugyanazt* a sorozatot adja — ezért tudunk reprodukálható futásokat csinálni. Innen a konstruktor `seed` paramétere: ezzel állítjuk be a generátor kiindulópontját.

Az eloszlás a `std::uniform_real_distribution<double>{0.0, 1.0}`: ez a motor nyers kimenetét egy egyenletes eloszlású, [0,1) közötti valós számmá alakítja. (Figyeld a `{0.0, 1.0}` **kapcsos zárójeles inicializálást** — ez a modern, egységes inicializálási szintaxis; a régi `(0.0, 1.0)` is menne, de a kapcsos forma egyértelműbb és bizonyos esetekben biztonságosabb.) A motor és az eloszlás külön él: a `dist_(rng_)` hívás úgy olvasandó, hogy „kérj az eloszlástól egy mintát, a motort használva forrásként". Ez a kettéválasztás teszi a `<random>`-t rugalmassá: ugyanaz a motor táplálhat többféle eloszlást.

A `total_bits_` és `flipped_bits_` futó számlálók a dashboard linkstatisztikájához kellenek — hány bit ment át összesen, és hányat billentettünk meg. Mindkettő `uint64_t`, hogy hosszú futásnál se csorduljon túl, és `= 0`-ról indul.

### A bitbillentés

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

A `transmit` *helyben* rontja az adatot (a puffert, amit kap). Végigmegy minden bájt minden bitjén, és minden bithez húz egy [0,1) közötti véletlen számot. Ha ez a szám a BER alá esik, az a bit „peches", és megbillen: a `data[i] ^= (1u << bit)` egy XOR-maszkkal átfordítja. Az XOR egy bittel pontosan a bitbillentés művelete — ha 0 volt, 1 lesz, ha 1 volt, 0. A valószínűségi logika is szép: ha a BER mondjuk 0,001, akkor minden bit átlagosan ezredannyi eséllyel billen, és a `dist_(rng_) < ber_` feltétel pont ezt valósítja meg.

Egy apró, de tudatos részlet az `1u << bit`: a szám `1u`, nem `1`. Az `u` utótag **előjel nélkülivé** teszi a literált. Eltolásnál ez jó szokás — előjeles típus eltolásánál bizonyos esetekben meghatározatlan a viselkedés, az előjel nélkülinél soha; ráadásul a célunk úgyis egy `uint8_t` maszk, így a `static_cast<uint8_t>` is ott van a biztonság kedvéért.

Ezzel megvan a tökéletlen rádió. A keretek most már megsérülhetnek átvitel közben — és pont ez ad munkát a projekt szívének, a dekódernek, amihez most megérkezünk.

---

## 9. A vevő szíve: a dekóder állapotgép (`decoder.hpp` / `decoder.cpp`)

Ez a projekt magja. Ha egyetlen részt akarsz tényleg a magadénak érezni, ez az. A dekóder egyetlen bájtot kap egyszerre — pontosan úgy, ahogy egy UART-megszakítás szállítaná —, és egy kis **állapotgéppel** rakja össze belőle a kereteket. Soha nem blokkol, és soha nem allokál: minden puffere fix méretű tagváltozó. Ez a két tulajdonság — a nem-blokkolás és az allokáció-mentesség — teszi, hogy *ugyanez a kód* elférne egy valódi mikrokontroller megszakításkezelőjében is.

Mielőtt a kódba ugranánk, álljunk meg a *szerződésnél*, mert ettől lesz a többi érthető.

### A szerződés: mi a bemenet, mi a kimenet

A **bemenet** egy bájtfolyam, bájtonként a `feed()`-en keresztül. Ez a folyam tartalmazhat tetszőleges szemetet a keretek előtt, között, sőt akár egy csonka keret belsejében is. A dekóder dolga, hogy ebből a kiszámíthatatlan zajból kihámozza az ép kereteket.

A **kimenet** kétféle. Egyrészt a visszafejtett `Telemetry` rekordok azokból a keretekből, amelyek CRC-je stimmel. Másrészt egy `DecoderStats` blokk, ami számolja, mi történt. A kettőt egy elegáns jelzés köti össze: a `feed()` **pontosan azon az egyetlen bájton ad vissza `true`-t, amelyik egy érvényes keretet lezárt**. Minden más híváskor `false`-t ad. Ha `true`-t kaptál, a frissen dekódolt rekord kiolvasható a `last_packet()`-tel. Ez a „minden hívás egy bájt, a `true` az érvényes keret jele" kontraktus pont az, ami illeszkedik a fő ciklushoz: a hívó egyesével tolja be a bájtokat, és amikor `true`-t lát, kiolvassa a friss mintát.

### A négy állapot

```cpp
enum class RxState {
    HuntSync,     // a szinkronszót keressük a folyamban
    ReadHeader,   // a 4 fejléc-bájtot gyűjtjük
    ReadPayload,  // a payload_len payload-bájtot gyűjtjük
    ReadCrc,      // a 2 CRC-bájtot gyűjtjük, majd ellenőrzünk
};
```

A vevő mindig pontosan egy állapotban van, és ezek mondják meg, „épp hol tartok". A `HuntSync` a kiindulás: a szemétben a szinkronszót vadásszuk. Ha megtaláltuk, a `ReadHeader`-ben összegyűjtjük a fejlécet; a `ReadPayload`-ban a hasznos adatot; a `ReadCrc`-ben a két CRC-bájtot, majd döntünk. Az `enum class` ugyanazért jó itt, mint a `Mode`-nál: típusbiztos, nem szennyez, és a `RxState::HuntSync` formával egyértelmű.

### A megfigyelő-számlálók

```cpp
struct DecoderStats {
    uint64_t bytes_seen     = 0;  // minden valaha betáplált bájt
    uint64_t frames_valid   = 0;  // CRC stimmelt
    uint64_t frames_crc_err = 0;  // CRC megbukott
    uint64_t frames_dropped = 0;  // sorszám-lyukból kikövetkeztetett hiány
    uint64_t resyncs        = 0;  // elvetett, hibás fejlécek / újraszinkronok
};
```

Ezeket a számlálókat olvassa a dashboard, és ezekre tesztel a tesztcsomag. Csak nőni tudnak, és együtt adják a link „egészségképét": hány bájt jött, hány keret volt ép, hány bukott CRC-n, hány keret veszett el (a sorszám-lyukakból), és hányszor kellett újraszinkronizálni. Ez a fajta **megfigyelhetőség** (observability) önmagában is a tesztelhetőség része — nem elég, hogy a kód jól működik, látni is kell tudni, hogy jól működik.

### Az osztály belső állapota — minden fix méretű

```cpp
private:
    RxState  state_   = RxState::HuntSync;
    uint32_t sync_sr_ = 0;  // görgő 32 bites regiszter: az utolsó 4 bájt

    uint8_t buf_[kHeaderSize + kMaxPayloadSize] = {};
    size_t  idx_ = 0;       // eddig buf_-ba gyűjtött bájtok

    uint8_t  payload_len_ = 0;
    uint16_t seq_         = 0;
    uint16_t rx_crc_      = 0;
    uint8_t  crc_bytes_   = 0;

    Telemetry last_{};
    uint16_t  last_seq_      = 0;
    bool      have_last_seq_ = false;

    DecoderStats stats_{};
```

Érdemes ezt végignézni, mert minden tagváltozó a beágyazott elveket testesíti meg. Az `state_` az aktuális állapot. A `sync_sr_` a görgő szinkronregiszter — erről mindjárt bővebben. A `buf_` a *kulcs*: ez tárolja a fejlécet és a payloadot együtt (mindent, amit a CRC fed), és **a legrosszabb esetre van méretezve** (`kHeaderSize + kMaxPayloadSize`, azaz 4+255). Ez a `kMaxPayloadSize` korlát itt fejti ki a hatását: bármekkora hosszt is hazudna egy elrontott fejléc, a puffer már eleve elég nagy, és a túl nagy hosszt úgyis elutasítjuk — soha nem lehet a `buf_`-on túlírni. A `{}`, illetve `{...}` a tagok után **érték-inicializál**: a `buf_` tömb nulláról indul, a `last_` és a `stats_` is tiszta. Nincs heap, nincs `new`, nincs `std::vector`; minden a fix méretű tagokban él.

### Az újraszinkronizálás — és egy tudatos döntés

```cpp
void Decoder::reset_to_hunt() {
    state_      = RxState::HuntSync;
    idx_        = 0;
    crc_bytes_  = 0;
    rx_crc_     = 0;
    // sync_sr_-t SZÁNDÉKOSAN nem nullázzuk: mindig az utolsó négy bájtot tartja,
    // így ha a következő bájton már új SYNC kezdődik (hát-hát keretek), azt is
    // elkapjuk. Nullázni is biztonságos volna, de így robusztusabb.
}
```

A `reset_to_hunt()` minden keret után (ép vagy sem) visszaviszi a gépet a `HuntSync` állapotba, és nullázza a keretenkénti munkaállapotot. De van benne egy **szándékos döntés**, ami beágyazott szemszögből finom: a `sync_sr_`-t *nem* nullázza. Miért? Mert a görgő regiszter mindig az utolsó négy beérkezett bájtot tartja, és ha két keret közvetlenül egymás után jön (hát-hát), akkor a következő keret szinkronszava részben már „a regiszterben lehet". Ha nulláznánk, az is helyes volna, de a meghagyásával egy hajszálnyit robusztusabb a gép. Ez a fajta apró, indokolt döntés a jó beágyazott kód jellemzője — és a kommentben ott is van az indoklása.

### A `feed()` — a folyamatos szinkrondetektálás

Most jön a lényeg. A `feed()` legelején, *minden állapotban lefutva*, ott a szinkrondetektálás:

```cpp
bool Decoder::feed(uint8_t byte) {
    ++stats_.bytes_seen;

    // --- Folyamatos SYNC-detektálás (MINDEN állapotban lefut) ---
    sync_sr_ = (sync_sr_ << 8) | byte;
    if (sync_sr_ == kSyncWord) {
        if (state_ != RxState::HuntSync) {
            ++stats_.resyncs;   // egy félkész keretet hagyunk ott az újraigazításhoz
        }
        state_     = RxState::ReadHeader;
        idx_       = 0;
        crc_bytes_ = 0;
        return false;
    }
    ...
```

Ez a néhány sor a dekóder legokosabb trükkje, úgyhogy lassítsunk le rajta. A `sync_sr_` egy 32 bites **görgő regiszter**. Minden beérkező bájtra balra toljuk 8-cal, és beOR-oljuk az új bájtot az alsó 8 bitbe. Az eredmény: a `sync_sr_` mindig pontosan az **utolsó négy beérkezett bájtot** tartalmazza. Képzeld el úgy, mint egy négy karakter széles ablakot, amin a bájtfolyam folyamatosan átcsúszik — te meg figyeled, mikor jelenik meg benne a varázsszó. Amint a regiszter egyenlő a szinkronszóval, megtaláltuk egy keret kezdetét, és átállunk `ReadHeader`-be.

A zsenialitás abban van, hogy ez a vizsgálat **minden állapotban** lefut, nem csak vadászat közben. Gondold végig, mit nyersz ezzel. Ha egy keret csonka maradt (mert mondjuk a zaj megette a felét), a naiv megoldás vakon tovább gyűjtene, és a következő keret szinkronszava elveszne benne — egy rossz keret így *több* elveszett keretté terjedne. Itt viszont, amint egy valódi szinkronszó megjelenik a folyamban — *bármelyik állapotban is vagyunk épp* —, a gép azonnal újraindul rá. Egy rossz keret soha nem nyeli el a következőt. Ráadásul ez egyben *elegánsabb* is: a teljes SYNC-kezelés egyetlen helyre kerül, a `feed()` tetejére, nem szóródik szét négy állapot között. (A korábbi terv egy „naiv v1" változatot vázolt, ami csak vadászat közben figyelt SYNC-re; pont ezért bukott a „resync csonkítás után" teszt, amíg a kód át nem állt erre a folyamatos változatra.)

Egy árat fizetünk érte, és a kód ezt őszintén dokumentálja: ha egy payload *véletlenül* tartalmazná a 4 bájtos szinkronmintát, az téves újraindítást okozna. De egy 32 bites markernél ennek esélye nagyjából **1 a 4 milliárdhoz** bájtpozíciónként — ezen a linken elhanyagolható. (Éles rendszerek ezt is kiküszöbölik fix keretpozíciókkal vagy bájt-töméssel; nálunk nem éri meg a bonyolítás.) Amikor pedig a SYNC nem vadászat közben, hanem egy félkész keret kellős közepén üt be, a `++stats_.resyncs` jelzi, hogy egy csonka keretet ott hagytunk, hogy újraigazodjunk — ez is megjelenik a dashboardon.

### A `feed()` — az állapotonkénti feldolgozás

Ha a beérkező bájt nem zárt le egy szinkronszót, akkor az állapotgép-rész következik:

```cpp
    switch (state_) {

    case RxState::HuntSync:
        // A SYNC-et fent kezeltük; itt nincs más dolog.
        return false;

    case RxState::ReadHeader: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize) {
            return false;  // még gyűlik a 4 fejléc-bájt
        }

        const uint8_t version = buf_[0];
        seq_         = get_u16(&buf_[1]);
        payload_len_ = buf_[3];

        // Védekező ellenőrzés: soha ne bízz a bemenetben.
        if (version != kVersionType || payload_len_ != kPayloadSize) {
            ++stats_.resyncs;
            reset_to_hunt();
            return false;
        }

        state_ = RxState::ReadPayload;
        return false;
    }

    case RxState::ReadPayload: {
        buf_[idx_++] = byte;
        if (idx_ < kHeaderSize + payload_len_) {
            return false;  // még gyűlik a payload
        }
        state_     = RxState::ReadCrc;
        crc_bytes_ = 0;
        rx_crc_    = 0;
        return false;
    }

    case RxState::ReadCrc: {
        rx_crc_ = static_cast<uint16_t>((rx_crc_ << 8) | byte);
        if (++crc_bytes_ < kCrcSize) {
            return false;  // kell a második CRC-bájt
        }

        const uint16_t calc = crc16_ccitt(buf_, kHeaderSize + payload_len_);
        if (calc == rx_crc_) {
            last_ = deserialize_telemetry(&buf_[kHeaderSize]);
            // ... sorszám-lyuk detektálás ...
            ++stats_.frames_valid;
            reset_to_hunt();
            return true;   // ez a bájt zárt le egy érvényes keretet
        }

        ++stats_.frames_crc_err;
        reset_to_hunt();
        return false;
    }
    }

    return false;  // elérhetetlen; a fordítót nyugtatja
```

Menjünk végig állapotonként. A **`HuntSync`** ágban nincs teendő — a szinkronszót már a `feed()` tetején kezeltük; ha idáig eljutottunk és a regiszter nem egyezett, csak várunk a következő bájtra.

A **`ReadHeader`** ágban begyűjtjük a bájtot a `buf_`-ba (`buf_[idx_++] = byte`). Amíg nincs meg mind a négy fejléc-bájt, csak gyűlünk. Amint megvan (`idx_ == kHeaderSize`), kifejtjük a fejlécet: az első bájt a `version`, a következő kettő a `seq_` (a `get_u16`-tal, big-endianban), a negyedik a `payload_len_`. És itt jön a **védekező lépés**, a kód egyik legfontosabb tanulsága: ha a verzió nem a várt `kVersionType`, *vagy* a hossz nem a várt `kPayloadSize`, akkor ez nem egy keret, amit értünk — vagy a szemét tartalmazta véletlenül a szinkronmintát, vagy a fejléc megsérült —, úgyhogy elvetjük és visszaszinkronizálunk. A `payload_len_` ellenőrzése most azért triviális, mert a hossz fix (13); de a *szokás* az igazi lecke. Ha valaha a hosszal puffert méreteznél és megbíznál egy elrontott óriási értékben, az puffer-túlcsordulás — pont az a fajta hiba, ami repülő szoftverben végzetes. A „soha ne bízz a bemenetben" itt épül be reflexszé.

Ha a fejléc rendben, átállunk `ReadPayload`-ra — és vedd észre, hogy az `idx_` **nem nullázódik**: a `kHeaderSize` értéken áll, és a payload-bájtok onnan folytatva töltik a `buf_`-ot, közvetlenül a fejléc után. Így a `buf_` végül pontosan a fejlécet és a payloadot tartalmazza, egymás után — vagyis pontosan azt a 17 bájtot, amit a CRC fed.

A **`ReadPayload`** ág ugyanígy gyűjt, amíg az `idx_` el nem éri a `kHeaderSize + payload_len_` értéket (17). Akkor átállunk `ReadCrc`-re, és nullázzuk a CRC-gyűjtéshez tartozó állapotot.

A **`ReadCrc`** ág a két CRC-bájtot rakja össze big-endianban (`rx_crc_ = (rx_crc_ << 8) | byte`) — pont ahogy a `get_u16` tenné, csak itt görgetve, bájtonként. Amikor mindkét CRC-bájt megvan (`++crc_bytes_ == kCrcSize`), jön az igazság pillanata: a kód **újraszámolja** a CRC-t a bufferelt fejléc+payload felett (`crc16_ccitt(buf_, kHeaderSize + payload_len_)`), és összeveti a kapottal. Ha egyeznek, a keret ép: deszerializáljuk a payloadot `Telemetry`-vé (a `buf_`-ból, a fejléc utáni résztől), megnöveljük a `frames_valid` számlálót, visszaszinkronizálunk, és — egyetlen helyen az egész kódban — **`true`-t adunk vissza**. Ha nem egyeznek, a keret útközben megsérült: növeljük a `frames_crc_err`-t, eldobjuk, visszaszinkronizálunk, `false`.

A `switch` után egy `return false` áll, „elérhetetlen" kommenttel. Erre azért van szükség, mert minden `case` `return`-nel végződik, de a fordító ezt nem mindig látja át, és figyelmeztetne, hogy „a függvény vége elérhető return nélkül". Ez a sor nyugtatja meg — apró udvariasság a `-Wall` felé.

### A sorszám-lyuk detektálása (és a körbefordulás kezelése)

Az érvényes keret ágában, a `frames_valid` növelése előtt, egy finom kis blokk ül:

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

Ez számolja ki, hány keret veszett el a linken. A logika: ha már volt korábbi érvényes keret (`have_last_seq_`), akkor a *várt* következő sorszám a `last_seq_ + 1`. Ha a most kapott `seq_` nem ennyi, akkor a kettő közti `gap` darab keret elveszett, és ennyivel nő a `frames_dropped`. A szépség a `uint16_t` aritmetikában van: mind a `last_seq_ + 1`, mind a `seq_ - expected` 16 biten, **körbefordulással** számolódik. Így ha a sorszám épp átfordult `65535`-ről `0`-ra, a kivonás akkor is a helyes (kicsi) lyukat adja, nem egy abszurd nagy számot. A `have_last_seq_` flag pedig azt kezeli, hogy a *legelső* keretnél még nincs mihez viszonyítani — ezért indul `false`-ról, és csak az első érvényes keret után válik `true`-vá. Ezt a tesztcsomag külön ellenőrzi: betáplál egy 0-s és egy 3-as sorszámú keretet (az 1-est és 2-est „elveszejtve"), és elvárja, hogy a `frames_dropped` pontosan 2 legyen.

### Az állapotgép egy ábrán

```
                    bármilyen bájt, ami NEM zár SYNC-et
                    ┌──────────────────────────────────┐
                    │                                   │
                    ▼                                   │
              ┌───────────┐                             │
   ┌─────────▶│ HuntSync  │─────────────────────────────┘
   │          └───────────┘
   │                │  a sync_sr_ == SYNC  (4 jó bájt a görgő ablakban)
   │                ▼
   │          ┌───────────┐   4 fejléc-bájt megvan,
   │          │ReadHeader │   a fejléc érvényes
   │          └───────────┘
   │                │
   │   rossz fejléc │  jó fejléc ──────────┐
   │   (++resyncs)  │                       ▼
   │◀───────────────┤                ┌───────────┐  payload_len bájt megvan
   │                │                │ReadPayload│──────────────┐
   │                │                └───────────┘               ▼
   │                │                                      ┌───────────┐
   │   CRC hiba     │   2 CRC-bájt megvan, CRC stimmel     │  ReadCrc  │
   │ (++crc_err)    │◀────────────────────────────────────└───────────┘
   │◀───────────────┘         (++frames_valid, feed() => true)
   │
   └──── És MINDEN állapotból: ha a görgő ablakban felbukkan egy valódi SYNC,
         azonnal vissza ReadHeader-be (ez a folyamatos szinkrondetektálás).
```

A lényeg, amit az ábra üzen: a gép **körkörös** — minden út visszavezet, vagy egy érvényes kerethez (és onnan a `HuntSync`-hez), vagy egy hibához (és onnan is a `HuntSync`-hez). És van egy „varázsnyíl", ami *bármelyik* állapotból a `ReadHeader`-be visz, amint egy igazi szinkronszó megjelenik. Emiatt a gépet **lehetetlen beékelni**: nincs olyan bemenet, amitől örökre megállna egy keret közepén.

### Egy konkrét végigfutás, bájtról bájtra

Tegyük konkréttá. Mondjuk a folyam ez: két szemétbájt (`00 FF`), aztán egy ép keret. Nézzük, mit csinál a dekóder:

- `00` érkezik: `sync_sr_` → `...00`, nem SYNC, `HuntSync`, `feed` → `false`.
- `FF` érkezik: `sync_sr_` → `...00FF`, nem SYNC, `HuntSync`, `false`.
- `1A` érkezik: `sync_sr_` → `...001AFF`... pontosabban `0x0000FF1A`, nem SYNC, `false`.
- `CF` érkezik: `sync_sr_` → `0x00FF1ACF`, nem SYNC, `false`.
- `FC` érkezik: `sync_sr_` → `0xFF1ACFFC`, nem SYNC, `false`.
- `1D` érkezik: `sync_sr_` → `0x1ACFFC1D` — **egyezik a SYNC-kel!** → `ReadHeader`, `idx_=0`, `false`.
- `01` érkezik: nem zár SYNC-et; `ReadHeader` ág, `buf_[0]=01`, `idx_=1`, `false`.
- `00`, `00` érkezik: `buf_[1]=00`, `buf_[2]=00`, `idx_=3`, `false`.
- `0D` érkezik: `buf_[3]=0D`, `idx_=4 == kHeaderSize` → fejléc kifejtve: `version=01` (jó), `seq_=0x0000`, `payload_len_=0x0D=13` (jó) → `ReadPayload`, `false`.
- a 13 payload-bájt érkezik: a `buf_[4..16]`-ba gyűlnek, `idx_` 5-ről 17-re nő; amikor `idx_==17` (`kHeaderSize+payload_len_`) → `ReadCrc`, `false`.
- az 1. CRC-bájt: `rx_crc_` felső fele beáll, `crc_bytes_=1`, `false`.
- a 2. CRC-bájt: `rx_crc_` teljes; `crc_bytes_=2`; a kód újraszámolja a CRC-t a `buf_` 17 bájtján, összeveti. Ha stimmel → deszerializálás, `frames_valid=1`, `reset_to_hunt()`, **`feed` → `true`**. A fő ciklus ezt látja, és kiolvassa a friss mintát.

Ha menet közben egy payload-bit megsérült volna, az utolsó lépésben a `calc != rx_crc_`, így `frames_crc_err=1`, és `feed` → `false` — a keret eldobva, a gép `HuntSync`-ben várja a következőt. Ennyire egyszerű és ennyire robusztus.

### Az öt szabály, ami a leckét adja

A dekóder mögött öt elv áll, és érdemes ezeket fejben tartani, mert ezek tehetők át bármilyen más beágyazott protokoll-feldolgozóba. **Egy:** minden keret után (ép vagy sem) vissza a kiindulóállapotba — soha ne ragadj be egy keret közepén. **Kettő:** bármilyen anomáliára (rossz verzió, rossz hossz, CRC-bukás) szinkronizálj vissza — a gépet legyen lehetetlen beékelni. **Három:** nincs dinamikus allokáció — fix pufferek a legrosszabb esetre. **Négy:** nincs blokkolás — a `feed` egy bájtot dolgoz fel és visszatér, a ciklus kívül van. **Öt:** tiszta reset — pontosan tudd, mit kell nullázni (`idx_`, `crc_bytes_`, `rx_crc_`) és mit érdemes meghagyni (a `sync_sr_`-t).

Ez a dekóder a projekt csúcspontja, és pont az a része, amit egy C3S-interjún érdemes felrajzolni egy táblára. A többi modul innentől „csak" az, ami táplálja és láthatóvá teszi. Lássuk, honnan jön a telemetria, amit keretez.

---

## 10. A műhold-szimulátor (`satellite.hpp` / `satellite.cpp`)

A dekóderhez kell valami, amit dekódolhat — egy adatforrás. Egy igazi cubesaten ez a sok érzékelő (feszültségosztó az akkun, hőmérők a paneleken, csillagkövető a helyzethez). Nálunk ezt a `Satellite` osztály helyettesíti, ami **élethű, simán változó** értékeket gyárt a küldetésidő függvényében: egy akku, ami töltődik-merül egy pálya alatt, panelhőmérsékletek, amik a napfény-árnyék váltakozással hullámzanak, egy lassan forgó helyzetszög, és egy üzemmód, ami követi az energiaállapotot. A cél nemcsak az, hogy *legyen* adat, hanem hogy a dashboardon **mozogjon** is, élethűen — mert egy mozdulatlan számoszlop nem győz meg senkit, egy lélegző telemetria viszont igen.

### A szinusz mint modellező eszköz

A fizikai folyamatok modellezésének legegyszerűbb és legkifejezőbb eszköze a szinuszhullám: periodikus, sima, és néhány paraméterrel pontosan szabályozható. Az anonim névtérben ott egy kis segédfüggvény, ami pont ezt adja:

```cpp
namespace {
constexpr double kPi = 3.14159265358979323846;

double wave(double t_s, double period_s, double center, double amp,
            double phase = 0.0) {
    return center + amp * std::sin(2.0 * kPi * t_s / period_s + phase);
}
}  // namespace
```

Két modern részlet azonnal előbukkan. Az egyik az **anonim (névtelen) névtér**. A régi C++-ban, ha egy függvényt csak az adott `.cpp` fájlon belül akartál láthatóvá tenni (hogy ne ütközzön más fájlok azonos nevű függvényeivel a linkelésnél), `static`-ot írtál elé. A modern C++ erre az anonim névteret ajánlja: a `namespace { ... }` blokkba tett minden név **belső linkelésű** lesz, vagyis a fájlon kívül láthatatlan. Itt a `wave` és a `kPi` csak a `satellite.cpp`-ben létezik — pont jó egy belső segédnek.

A másik az **alapértelmezett függvényargumentum**: a `phase = 0.0`. Ha a hívó nem ad fázist, automatikusan nulla lesz. Ez valójában régi C++-ból is ismerős lehet, de itt szépen mutatja a hasznát: a legtöbb hullám fázis nélkül hívható (`wave(t, 40.0, 3700.0, 300.0)`), és csak ahol kell, ott adunk meg fázist (az OBC hőmérséklethez). A `std::sin` a `<cmath>`-ból jön, a `kPi` pedig `constexpr` — ugyanaz a fordítási idejű konstans, mint mindenhol.

A `wave` képlete a tankönyvi szinusz: a `center` az átlagérték, az `amp` a kilengés amplitúdója, a `period_s` a periódus másodpercben, a `phase` pedig eltolja a hullámot időben. Vagyis a függvény egy `center ± amp` között, `period_s` periódussal lengő, `t_s` időben kiértékelt szinuszt ad.

### Az érzékelők modellezése

```cpp
Telemetry Satellite::sample(uint32_t t_ms) {
    const double t = t_ms / 1000.0;  // küldetésidő másodpercben

    Telemetry tm;
    tm.timestamp_ms = t_ms;

    // Akkufeszültség: lassú töltés/merülés (demóhoz ~40 s-ra gyorsítva),
    // 3700 mV közép, ±300 mV, kis érzékelőzajjal.
    tm.batt_mv = static_cast<uint16_t>(
        wave(t, 40.0, 3700.0, 300.0) + 8.0 * noise_(rng_));

    // Panelhőmérsékletek: napfény/árnyék lengés. Az OBC kissé késik fázisban
    // és hűvösebb. 0.1 °C lépés, tehát 235 = 23.5 °C.
    tm.temp_eps_c10 = static_cast<int16_t>(
        wave(t, 30.0, 200.0, 150.0) + 2.0 * noise_(rng_));
    tm.temp_obc_c10 = static_cast<int16_t>(
        wave(t, 30.0, 180.0, 120.0, -0.6) + 2.0 * noise_(rng_));

    // Helyzetszög: lassú bukfenc ±180°. 0.01° lépés.
    const double deg = wave(t, 25.0, 0.0, 180.0);
    tm.attitude_cdeg = static_cast<int16_t>(deg * 100.0);

    // Üzemmód: alacsony akkunál Safe, különben körbejár az aktív módokon.
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

Ez a függvény adja a küldetésidő egy adott pillanatához tartozó mintát. Minden érzékelő egy `wave` hívás, gondosan megválasztott paraméterekkel. Az **akku** 3700 mV körül leng ±300 mV-ot, 40 másodperces periódussal (egy valódi pálya órákig tartana, de demóhoz felgyorsítottuk, hogy nézhető legyen). Az **EPS-panel** hőmérséklete 20 °C körül (`center=200`, azaz 20,0 °C) leng 30 másodperces periódussal; az **OBC-panel** ugyanígy, de egy `−0.6` rad fázis-eltolással (kicsit *késik* az EPS után, ahogy a valóságban is, mert lassabban melegszik) és alacsonyabb átlaggal — fizikailag hihető, hogy a fedélzeti számítógép panele kicsit más hőképet mutat, mint az energiarendszeré. A **helyzetszög** ±180°-ot söpör végig 25 másodperces periódussal, egy lassú bukfencet utánozva.

Mindegyikre rákerül egy kis **érzékelőzaj** is: a `noise_(rng_)` egy [−1, 1] közötti véletlen szám (ez is `std::uniform_real_distribution` és `std::mt19937`, ahogy a csatornában), megszorozva egy kis amplitúdóval (`8.0`, `2.0`). Ez teszi a görbéket „életszerűvé" — nem tökéletesen sima matematikai szinuszok, hanem enyhén remegő mérések, mint a valódi érzékelők. A `static_cast`-ok pedig a lebegőpontos eredményt visszaalakítják a `Telemetry` egész mezőivé — itt történik a „nyers fizikai érték → tárolt skálázott egész" lépés, ami a beágyazott ADC-feldolgozás párja.

Az **üzemmód** logikája szépen összeköti az adatokat: ha az akku 3450 mV alá esik, a műhold `Safe` módba megy (minimális fogyasztás, mint a valóságban egy lemerülő szondánál); különben a `(t_ms / 8000) % 3` kifejezés 8 másodpercenként körbeforgatja a három aktív módot (`Nominal`, `Payload`, `Comms`). Ez az egész egy apró **állapotmodell** a műhold viselkedésére — és mivel az akku szinuszosan merül, néha tényleg le is megy 3450 alá, így a `Safe` mód is megjelenik a dashboardon, nem csak elméletben.

Az egész osztály lényege egy mondatban: determinisztikus (ugyanaz a `seed` ugyanazt a telemetriát adja), de élethű forrás, ami a dekódernek munkát, a dashboardnak pedig látványt ad. Most lássuk, hogyan lesz ebből kép a képernyőn.

---

## 11. A látványos kimenet: a dashboard (`dashboard.hpp` / `dashboard.cpp`)

Ez a leghosszabb forrásfájl (366 sor), de ne ijedj meg: a java része ismétlődő rajzolgatás, és pont ezt fogja a lambdák segítségével tömöríteni. A dashboard a projekt „nyilvánvaló eredménye": egy teljes képernyős, helyben frissülő ANSI-kijelző, amin valós időben látod az értékek mozgását, és látod, ahogy a CRC-hibaszámláló kúszik fölfelé, miközben a jó keretek továbbra is átmennek. Itt jön elő a legtöbb lambda, és itt lakik egy nagyon tanulságos hiba is, amit a fejlesztés közben elkaptunk.

### Hogyan néz ki valójában

Mielőtt a kódba mennénk, nézd meg, mit gyárt. Ez egy *valódi* pillanatkép a programból, egy romlott (BER = 10⁻³) linken, nyolc másodperc után (a színeket itt elvettem, hogy szövegként is olvasható legyen):

```
╔══════════════════════════════════════════════════════════════╗
║         CUBESAT TELEMETRY LINK   //   GROUND STATION         ║
╠══════════════════════════════════════════════════════════════╣
║  MISSION TIME 00:00:08            MODE      NOMINAL          ║
║                                                              ║
║  Battery      3.98 V       [█████████████████░] 3.40-4.00 V  ║
║  EPS temp     +35.0°C      [███████████████░░░]              ║
║  OBC temp     +28.2°C      [█████████████░░░░░]              ║
║  Attitude     +165.2°      [█████████████████░] -180..+180   ║
╠══════════════════════════════════════════════════════════════╣
║  LINK                                                        ║
║  Bit error rate 1.0e-03    [███████████░░░░░░░] DEGRADED     ║
║  Bits flipped   15 / 11776                                   ║
╠══════════════════════════════════════════════════════════════╣
║  DECODER                                                     ║
║  Frames valid     51       ✓                                 ║
║  CRC errors       9        ✗                                 ║
║  Dropped (gaps)   12                                         ║
║  Resyncs          2                                          ║
║  Frame yield      85.0 %   [███████████████░░░]              ║
╚══════════════════════════════════════════════════════════════╝
```

Ez az egész egy szövegből rajzolt „műszerfal": dobozkeret, gauge-sávok (a tele `█` és üres `░` blokkokból), élő üzemmód, és három blokk — a telemetria, a link állapota, a dekóder statisztikái. Élesben a `█` blokkok zölden/sárgán/pirosan világítanak az érték függvényében, és az egész másodpercenként többször frissül. Most nézzük, hogyan épül ez fel.

### Az ANSI-vezérlőszekvenciák

A terminál „rajzolásának" eszköze az ANSI escape-szekvencia: speciális karaktersorozatok, amiket a terminál nem kiír, hanem *parancsként* értelmez (szín, kurzormozgatás, képernyőtörlés). A fájl ezeket konstansként definiálja:

```cpp
constexpr const char* kReset   = "\x1b[0m";
constexpr const char* kBold    = "\x1b[1m";
constexpr const char* kGreen   = "\x1b[32m";
// ... kDim, kRed, kYellow, kCyan, kMagenta
```

A `\x1b` az ESC karakter (27-es kód); a `[32m` mondja meg, hogy „mostantól zöld". A `begin()` és `end()` ezeket használja a képernyő előkészítésére:

```cpp
void Dashboard::begin() {
    std::fputs("\x1b[2J\x1b[?25l", stdout);  // képernyő törlése, kurzor elrejtése
    std::fflush(stdout);
}
void Dashboard::end() {
    std::fputs("\x1b[?25h\n", stdout);        // kurzor visszamutatása
    std::fflush(stdout);
}
```

A `\x1b[2J` letörli a képernyőt, a `\x1b[?25l` elrejti a kurzort (hogy ne villogjon zavaróan a frissülő kijelzőn), a `\x1b[?25h` pedig a végén visszahozza. A `render()` legelején pedig egy `\x1b[H` viszi a kurzort a bal felső sarokba — így minden frissítésnél *fölülírjuk* az előző képet a régi helyén, ahelyett hogy görgetnénk. Ettől lesz „helyben frissülő" a dashboard.

### A `fmt` — printf egy std::stringbe

```cpp
std::string fmt(const char* f, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return std::string(buf);
}
```

Ez egy apró kényelmi függvény: úgy formáz, mint a `printf`, de a végeredményt egy `std::string`-ben adja vissza, nem a képernyőre írja. A **változó argumentumlista** (`...`, `va_list`, `va_start`, `va_end`) régi C-eszköz, valószínűleg ismerős; a modern csavar a `std::vsnprintf` (a méretkorlátos, biztonságos `sprintf`-változat, ami nem csordul túl a `buf`-on) és a `std::string`-be csomagolt visszatérés. A `buf[96]` egy fix méretű stack-puffer — egyetlen sor formázott szövege bőven elfér benne, és nincs heap-allokáció a formázáshoz.

### A `Row` — és a UTF-8 szélesség problémája

Most jön a fájl legtanulságosabb ötlete. A dobozt úgy rajzoljuk, hogy minden sornak pontosan ugyanannyi **látható oszlopnak** kell lennie, különben a jobb oldali `║` szegély „beljebb csúszik" egyes soroknál, és csorba lesz a doboz. A naiv megoldás az volna, hogy számoljuk a sztring **bájthosszát**. Csakhogy itt egy csapda van: a dashboard tele van **többbájtos UTF-8 karakterekkel**, amik *látható szélessége* nem egyezik a *bájthosszukkal*. A `█` blokk három bájt, de egy oszlop széles. A `°` fok-jel két bájt, egy oszlop. A `✓` pipa három bájt, egy oszlop. Ha bájtokat számolnál, ezeknél a soroknál túl szélesnek hinnéd a tartalmat, és túl keveset töltenél ki — a doboz elcsúszna.

A megoldás egy kis `Row` struktúra, ami **külön tartja a bájtpuffert és a látható oszlopszámot**:

```cpp
struct Row {
    bool        color;
    std::string buf;
    int         vis = 0;   // látható oszlopok száma, a bájthossztól függetlenül

    // Sima ASCII (a bájthossz = az oszlopszám).
    void asc(const std::string& s) { buf += s; vis += static_cast<int>(s.size()); }
    // Egy glyph, aminek a látható szélességét MI mondjuk meg (pl. többbájtos blokk).
    void glyph(const std::string& s, int w) { buf += s; vis += w; }
    // Nulla szélességű ANSI kód; csak ha a szín be van kapcsolva.
    void ansi(const char* code) { if (color) buf += code; }
    // Térközzel feltölt az abszolút `target` oszlopig.
    void pad(int target) { while (vis < target) { buf += ' '; ++vis; } }
    // ... value(), gauge() ...
};
```

Itt a kulcs a `vis` mező és a kétféle szövegfűzés. Az `asc()` sima ASCII-hez való, ahol a bájthossz tényleg egyenlő az oszlopszámmal (`vis += s.size()`). A `glyph()` viszont azokhoz a többbájtos karakterekhez, ahol *mi mondjuk meg explicit*, hány oszlop széles (`vis += w`, ahol `w` általában 1). Az `ansi()` a színkódokat fűzi hozzá, de a `vis`-t **nem** növeli — mert egy színkód láthatatlan, nulla oszlop széles. A `pad()` pedig a tényleges igazítás motorja: térközöket rak, amíg a látható oszlopszám el nem éri a megadott abszolút oszlopot.

Ez az utolsó pont a másik fél-trükk: minden mező egy **abszolút oszlopra** van igazítva (`kLabelCol=2`, `kValueCol=15`, `kBarCol=28`, `kNoteCol=49`), nem egymáshoz képest. Vagyis nem azt mondjuk, hogy „a címke után tegyél 5 szóközt", hanem azt, hogy „pad-elj a 15. oszlopig, bárhol is tartasz". Így ha egy érték rövidebb vagy hosszabb, a következő mező akkor is pontosan a helyén marad. A `glyph()` explicit szélesség és a `pad()` abszolút oszlop együtt teszik a dobozt **pixel-pontossá**, a többbájtos UTF-8 ellenére.

> **A hiba, amit elkaptunk.** A fejlesztés közben a `Frames valid` és a `CRC errors` sorok véletlenül **két oszloppal rövidebbek** voltak — a doboz jobb széle ott becsúszott. Az ok pontosan ez: a kód eredetileg a `value()`-t hívta a pipa/kereszt jelekre, ami az `asc()`-on keresztül a *bájthosszt* számolta, és a `✓` három bájt. A javítás: a jeleket a `glyph(mark, 1)`-gyel kell kiírni, ami kimondja, hogy „ez egy oszlop". Ránézésre nem látszott a hiba (a `✓` egy oszlopnak *tűnik*), csak amikor a tényleges oszlopszélességet megmértük. Ez a klasszikus lecke arról, hogy **UTF-8-nál a bájthossz nem a látható szélesség** — és pont ezért van szükség a `Row` absztrakciójára.

### A lambdák — a render() motorja

A `render()` függvény tele van **lambdákkal**, és mivel ez a modern C++ egyik legfontosabb eleme, amit nem ismersz, álljunk meg rajta rendesen.

Egy **lambda** egy névtelen függvény, amit helyben, a kód közepén definiálsz. A szintaxisa `[capture](paraméterek) { test }`. A `[capture]` rész — a „capture clause" — mondja meg, hogy a lambda hozzáférhet-e a környező változókhoz, és hogyan. A `[&]` azt jelenti: „kapj el mindent **referencia szerint**", vagyis a lambda közvetlenül használhatja (és módosíthatja) a környező függvény változóit. A `[=]` érték szerint másolná őket, a `[]` pedig semmit nem kapna el. Itt végig `[&]`-t látsz, mert a rajzoló segédeknek a közös `out` sztringhez és a `color_` taghoz kell hozzáférniük.

Nézd meg ezt a hármat a `render()` elejéről:

```cpp
auto rule = [&](const std::string& s) { out += s; out += "\x1b[K\n"; };
auto emit = [&](Row& r) {
    r.pad(kInner);
    out += "║";
    out += r.buf;
    out += "║\x1b[K\n";
};
auto new_row = [&] { Row r; r.color = color_; return r; };
```

A `rule` egy lambda, ami egy vízszintes vonalat (pl. a doboz tetejét) fűz az `out`-hoz, lezárva egy „sor végéig törlés" kóddal (`\x1b[K`). Az `emit` egy `Row`-t zár le: a tartalmat a `kInner` szélességig pad-eli, két oldalról `║` szegélyt tesz, és új sort kezd. A `new_row` pedig egy friss `Row`-t gyárt, beállítva a szín-flaget. Mindhárom `auto` típusú — a lambda típusát a fordító találja ki, te nem írod le (nem is tudnád szépen leírni). A hívásuk teljesen úgy néz ki, mint egy függvényé: `rule(top)`, `emit(r)`, `new_row()`.

Miért jó ez? Mert a `render()` ugyanazt a rajzolási mintát ismétli sokszor (címke, érték, gauge, megjegyzés), és a lambdák ezt **tömör, helyi segédfüggvényekbe** zárják, amik osztoznak a környező állapoton (`out`, `color_`), anélkül hogy külön, fájlszintű függvényeket kéne írni hat paraméterrel. A régi C++-ban vagy sok apró tagfüggvényt írtál volna, vagy egy csomó paramétert passzolgattál volna; a lambda + `[&]` capture ezt egy helyre, olvashatóan hozza.

A nehezebb rajzolásokra is van lambda. A `channel_row` egy teljes telemetria-sort rajzol (címke, érték, gauge, megjegyzés); a `temp_row` egy hőmérséklet-sort a fok-jellel (itt látod a `glyph("°", 1)`-et akcióban); a `counter_row` egy számláló-sort a pipa/kereszt jellel (itt a `glyph(mark, 1)` a javított, helyes változat). Ezek mind `[&]`-t kapnak el, és a `render()` testében egyszerűen meghívódnak a megfelelő adatokkal. Például:

```cpp
counter_row("Frames valid", stats.frames_valid, kGreen, "✓");
counter_row("CRC errors", stats.frames_crc_err,
            stats.frames_crc_err ? kRed : kDim, "✗");
```

A `CRC errors` sornál egy szép apróság: a szín a `stats.frames_crc_err ? kRed : kDim` feltételtől függ — ha *van* CRC-hiba, pirossal kiabál, ha nincs, halványan marad. Ettől lesz a dashboard *beszédes*: ránézésre látod, baj van-e.

### A link- és a hozam-logika

Két helyen van egy kis „okosság" a megjelenítésben. A `link_info(ber)` egy bithiba-rátát fordít le emberi címkévé, színné és egy 0..1 közötti „súlyosságra" (ami a link-gauge-ot hajtja), méghozzá **logaritmikus skálán**, mert a BER tartománya (10⁻⁶-tól 10⁻¹-ig) több nagyságrendet ölel át — lineáris skálán semmit nem látnál. A címkék lépcsőzetesek: `CLEAN`, `GOOD`, `DEGRADED`, `POOR`, `CRITICAL`. A `Frame yield` (keret-hozam) pedig a `frames_valid / (frames_valid + frames_crc_err)` arányt mutatja százalékban, és a színe zöld/sárga/piros aszerint, hogy 95% fölött, 80–95% között, vagy az alatt van. Ez a két szám együtt mondja el a link „egészségét": mennyire zajos a csatorna, és ebből mennyi keret jut át épségben.

A dashboard tehát az a réteg, ahol a száraz számokból *látvány* lesz — és ahol a UTF-8 szélesség-lecke megtanítja, miért nem mindig a bájthossz a látható szélesség. A következő kérdés: mi köti össze az egész csövet, és kezeli a parancssort? Ez a `main` dolga.

---

## 12. A fő ciklus és a parancssor (`main.cpp`)

A `main.cpp` az a hely, ahol az öt modul egyetlen csővé áll össze, pontosan úgy, ahogy egy földi állomás szoftvere ülne egy valódi rádiólink végén. A fájl tetején lévő komment maga a térkép:

```
Satellite.sample()  ->  Framer.encode()  ->  NoisyChannel.transmit()
     ->  Decoder.feed() bájtonként  ->  Dashboard.render()
```

### A tiszta kilépés: `volatile std::sig_atomic_t`

```cpp
volatile std::sig_atomic_t g_stop = 0;
void on_sigint(int) { g_stop = 1; }
```

A program egy végtelen ciklusban fut (élő módban), és Ctrl-C-vel állítható le. A Ctrl-C egy `SIGINT` szignált küld, amit a `on_sigint` kezel. De a szignálkezelő egy **kényes hely**: a program tetszőleges pontján, aszinkron módon hívódhat meg, ezért szinte semmit nem szabad benne csinálni. Az *egyetlen* típus, amit biztonságosan írhatsz egy szignálkezelőből, a `std::sig_atomic_t` — ez a szabvány által garantáltan „oszthatatlanul" (atomikusan) írható egész. A `volatile` kulcsszó pedig megmondja a fordítónak, hogy ezt a változót **ne optimalizálja ki**, ne tegye regiszterbe, mert „kívülről" (a szignálkezelőből) is változhat — minden olvasásnál tényleg a memóriából olvassa. Így a fő ciklus a `while (!g_stop)` feltételben mindig a friss értéket látja, és Ctrl-C után tisztán kilép, helyreállítva a terminált (visszahozza a kurzort). Ez a `volatile sig_atomic_t` páros a szignálkezelés tankönyvi, helyes idiómája.

### A beállítások és az argumentumkezelő

```cpp
struct Options {
    double   ber       = -1.0;   // <0: automatikus sweep
    uint32_t seed      = 1;
    int      rate      = 8;      // telemetria-keret/másodperc
    double   duration  = 0.0;    // 0: fuss Ctrl-C-ig
    bool     once      = false;  // egyetlen pillanatkép, majd kilép
    bool     color     = true;
    bool     garbage   = true;   // sorzaj a keretek közé
};
```

Az `Options` egy egyszerű beállítás-rekord, alapértelmezett tagértékekkel (ugyanaz a C++11-es minta, mint a `Telemetry`-nél). Az érdekes az a konvenció, hogy a `ber = -1.0` egy „nincs megadva" jelzőérték: ha negatív marad, a program az automatikus link-romlási menetrendet (`ber_sweep`) használja; ha a felhasználó megad egy konkrét értéket, az lesz.

A `ber_sweep` maga egy kis menetrend, ami időben lépteti a link minőségét, hogy a demó önjáró legyen:

```cpp
double ber_sweep(double t_s) {
    const double phase = std::fmod(t_s, 32.0);
    if (phase < 8.0)  return 0.0;    // CLEAN: semminek sem szabad elbuknia
    if (phase < 16.0) return 1e-4;   // GOOD: ritka hiba
    if (phase < 24.0) return 1e-3;   // DEGRADED: a CRC kezd kereteket kifogni
    return 5e-3;                     // POOR: sok keret elutasítva
}
```

Egy 32 másodperces ciklusban végigviszi a linket a tisztától a zajosig és vissza. Ez nem véletlen: pont ettől *látványos* a dashboard, mert a saját szemeddel látod, ahogy a CRC-hibaszámláló nyugton van tiszta linken, majd elkezd kúszni, ahogy romlik a csatorna — de a jó keretek végig átmennek.

Az argumentumkezelő egy kicsi, kézi parser, és benne egy beágyazott lambdával:

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
        else if (!std::strcmp(a, "--seed")) { /* ... */ }
        // ... --rate, --duration, --once, --no-color, --no-garbage, --help
    }
    return true;
}
```

A `need_value` egy **lambda**, ami `[&]`-vel elkapja a környező `i`, `argc`, `argv`, `exit_code` változókat, és kezeli azt az ismétlődő mintát, hogy „a következő argumentumot olvasd be értékként, vagy hibázz, ha nincs". A `std::strcmp` a C-stringek összehasonlítása (0-t ad, ha egyeznek — innen a `!std::strcmp(...)` „ha egyenlő" olvasat), a `std::atof` pedig szöveget alakít lebegőpontos számmá. Ez a kézi parser tudatosan minimalista — egy beágyazott demónál nem kell argumentumkönyvtár, pár `strcmp` bőven elég.

### A fő ciklus, lépésről lépésre

A `main` először felépíti a komponenseket, és itt egy szép részlet bukkan elő — a **levezetett magok**:

```cpp
NoisyChannel channel(opt.ber < 0.0 ? 0.0 : opt.ber, opt.seed ^ 0xA5A5u);
std::mt19937 noise_rng(opt.seed ^ 0x1234u);
```

Három különböző véletlen-forrás van a programban: az érzékelőzaj (a `Satellite`-ben), a csatorna bitbillentése (a `NoisyChannel`-ben), és a „sorzaj" a keretek közé (a `main`-ben). Mindháromnak **független, de reprodukálható** sorozatra van szüksége. A trükk: ugyanabból a felhasználói `seed`-ből indulnak, de mindegyik egy *más konstanssal XOR-olva* (`^ 0xA5A5`, `^ 0x1234`), így a sorozataik nem korrelálnak egymással, miközben az egész futás egyetlen `seed`-del megismételhető. Elegáns és olcsó megoldás a reprodukálható, de független véletlenre.

Aztán jön a ciklus, és a komment elárulja a kulcsdöntést: a küldetésidő a *telemetria-ütemből* számolódik, nem a falióráról:

```cpp
const uint32_t t_ms = static_cast<uint32_t>(frame_idx * 1000 / opt.rate);
```

Ez azt jelenti, hogy a telemetria és az óra **determinisztikus** marad, függetlenül attól, hogy az operációs rendszer mennyire pontosan ütemezi a programot. A 8-as keretsebességnél minden keret pontosan 125 ms-nyi küldetésidőt képvisel, akkor is, ha a valós idő kicsit csúszik. Ettől megismételhető a futás.

A ciklus magja pontosan az öt lépés. Először a műhold ad egy mintát (`satellite.sample(t_ms)`), aztán a keretező berakja borítékba (`framer.encode`). Opcionálisan beszúr néhány véletlen „sorzaj" bájtot a keret elé (12% eséllyel 1–3 bájtot), hogy a dekóder szinkronkeresése és újraszinkronja *élesben* is gyakorlatot kapjon — ez teszi a `Resyncs` számlálót nem-nullává a dashboardon. Majd a csatorna megrontja a keretet (`channel.transmit`), és a megérkezett bájtokat egyesével betáplálja a dekóderbe:

```cpp
channel.transmit(frame, n);
for (size_t k = 0; k < n; ++k) {
    if (decoder.feed(frame[k])) {
        last_decoded = decoder.last_packet();
        have_decoded = true;
    }
}
```

Pontosan úgy, ahogy egy UART szállítaná a vett bájtokat: egyesével, és amikor a `feed` `true`-t ad, kiolvassuk a friss mintát. Végül a dashboard kirajzol (`dashboard.render`), és a program vár a következő keretig:

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(1000 / opt.rate));
```

Itt a `<chrono>` és a `<thread>` modern könyvtárak dolgoznak. A `std::chrono::milliseconds(...)` egy **típusos időtartam** — nem egy nyers szám, hanem egy érték, aminek a fordító ismeri a mértékegységét (milliszekundum). A `std::this_thread::sleep_for(...)` pedig altatja a szálat ennyi időre. A régi C++-ban ehhez platformfüggő hívást (`Sleep`, `usleep`) használtál; a modern változat hordozható és típusbiztos — nem keverhetsz össze másodpercet milliszekundummal, mert a típus megvéd tőle.

Két apró, de fontos részlet zárja a `main`-t. Az `isatty(STDOUT_FILENO)` megnézi, hogy a kimenet egy igazi terminál-e; ha fájlba irányítják (pl. `> log.txt`), akkor a színek és kurzortrükkök csak szemetet okoznának, ezért a program **magától kikapcsolja** őket. És a pillanatkép (`--once`) mód: ilyenkor a program gyorsan legyárt egy fix számú keretet, majd *egyszer* rajzol — pont ezzel készült a fenti dashboard-kép.

Ezzel a cső teljes és működik. De honnan tudjuk *biztosan*, hogy minden réteg helyes? Erre való a tesztcsomag.

---

## 13. Tesztelés: a teszt-létra (`tests/test_main.cpp`) és a Makefile

A kiírás külön kiemeli a **szoftvertesztelési módszereket**, és a projekt erre egy önálló választ ad: egy „teszt-létrát", ami **alulról fölfelé**, fokról fokra bizonyítja, hogy minden réteg helyesen működik. Minden teszt **egyetlen konkrét tulajdonságot** igazol, és a sorrend a függőségeket követi: előbb a CRC, aztán a szerializáció, a keretezés, a dekóder állapotgép, és végül egy „kínzóteszt". Ha minden fok zöld, a vevő egy szeméttel teli, bitbillentő linken is helyesen viselkedik — pont az a robusztusság, amit a kiírás kér.

### A `CHECK` makró és három preprocesszor-trükk

```cpp
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

Ez a kis makró a tesztelés motorja, és három klasszikus preprocesszor-fogást rejt. Az első a **`do { ... } while (0)`** csomagolás. Ez egy bevett C-trükk, amitől a többsoros makró egyetlen utasításként viselkedik. Ha a `do/while(0)` nélkül írnád, és valaki `if (x) CHECK(...); else ...` formában használná kapcsos zárójelek nélkül, a makró szétesne és elrontaná az `if/else` szerkezetet. A `do { ... } while(0)` egyetlen, pontosvesszővel zárható blokká fogja össze — bárhol biztonságosan használható.

A második a **`#cond`**, a „stringizálás". A `#` operátor a preprocesszorban a makró argumentumát *szöveggé* alakítja. Vagyis ha azt írod, hogy `CHECK(crc16_ccitt(...) == 0x29B1)`, és a feltétel megbukik, a hibaüzenetben szó szerint ott lesz a `crc16_ccitt(...) == 0x29B1` szöveg — pontosan látod, *melyik* ellenőrzés bukott el, anélkül hogy kézzel le kéne írnod.

A harmadik a **`__FILE__` és `__LINE__`**: ezek beépített preprocesszor-makrók, amik a *forrásfájl nevét* és az *aktuális sorszámot* adják. Így a hibaüzenet pontosan megmutatja, hol bukott a teszt — egy fájl:sor hivatkozással, amire akár rá is kattinthatsz a szerkesztőben. Együtt ez a három fogás egy meglepően kényelmes mini-tesztkeretet ad alig tíz sorból, külső könyvtár nélkül.

### A létra fokai

A tesztek pontosan a cső rétegeit követik, alulról fölfelé. A `test_crc` a már ismert `0x29B1` ellenőrzőértéket bizonyítja (és az üres bemenet `0xFFFF`-jét). A `test_serialization` a körbejárást (`encode → decode` visszaadja az eredetit, a negatív mezőkkel együtt), és külön ellenőrzi a big-endian bájtsorrendet: a `batt_mv = 3700 = 0x0E74` tényleg `0x0E`, majd `0x74` bájtként jelenik meg. A `test_framer` a keret szerkezetét nézi (szinkronszó elöl, a fejléc-bájtok a helyükön, a sorszám nő). 

Innentől jön a dekóder, és a tesztek szépen lefedik a viselkedését. A `test_decode_roundtrip` bizonyítja, hogy **csak az utolsó bájt** zárhat le egy keretet (`feed` egyetlen `true`-t ad, a legvégén) — ez maga a szerződés. A `test_sync_hunt` szemétbájtokat szúr a keret elé, és elvárja, hogy a HUNT átcsússzon rajtuk a szinkronszóig. A `test_two_frames` két egymást követő keretet ellenőriz (két érvényes keret, nő a sorszám). A `test_crc_catches_payload_flip` és a `test_crc_catches_crc_flip` egy-egy bitet billent — előbb a payloadban, aztán magában a CRC-mezőben —, és bizonyítja, hogy a CRC mindkettőt elkapja. 

A két legfontosabb robusztusság-teszt a `test_resync_after_truncation` és a `test_bad_header_rejected`. Az első egy **félbevágott keretet**, némi szemetet, majd egy teljes keretet küld, és elvárja, hogy a teljes keret *mégis* dekódolódjon — vagyis a folyamatos szinkrondetektálás működik, a csonka keret nem nyeli el a következőt. (Pont ez a teszt bukott a „naiv v1" dekóderrel, és pont ez vezetett a folyamatos SYNC-detektálásra.) A második egy szándékosan **rossz verzió-bájttal** épített keretet küld, majd egy jót, és bizonyítja, hogy a védekező fejléc-ellenőrzés elveti a rosszat (nő a `resyncs`), de a jó keret utána átmegy. A `test_sequence_gap` a lyuk-detektálást igazolja: 0-s és 3-as sorszámú keret után a `frames_dropped` pontosan 2.

A létra teteje a `test_torture` — a kínzóteszt:

```cpp
void test_torture() {
    Decoder d;
    std::mt19937 rng(0xC0FFEE);
    for (int i = 0; i < 1'000'000; ++i) {
        d.feed(static_cast<uint8_t>(rng() & 0xFF));
    }
    CHECK(d.stats().bytes_seen == 1'000'000);
    // ... és egy valódi keret utána még mindig dekódolódik
}
```

Egymillió véletlen bájtot tol a dekóderbe, és bizonyítja, hogy **nem omlik össze és nem ékelődik be** — a végén egy valódi keret még mindig helyesen dekódolódik. Ez szó szerint a kiírás „tesztelési módszerek" sora teljesítve: bebizonyítjuk, hogy a parser tetszőleges szemétre is stabil marad. Egy modern apróság itt a **`1'000'000`** írásmód: a C++14-től az aposztróf számjegy-elválasztóként használható, pusztán az olvashatóságért — az egymilliót így ránézésre is felismered, nem kell a nullákat számolnod.

Amikor lefuttatod (`make test`), a végén ez áll: **`ALL 42 CHECKS PASSED`**. Tizenkét tesztcsoport, negyvenkét konkrét ellenőrzés, mind zöld.

### A Makefile

```makefile
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude -MMD -MP
```

A Makefile fordítja a fő programot is, a teszteket is. A fordítási kapcsolók beszédesek: a `-std=c++17` rögzíti a nyelvi szabványt; a `-Wall -Wextra -Wpedantic` **minden figyelmeztetést bekapcsol** (ezért volt fontos a sok explicit `static_cast` — egy néma szűkítés itt figyelmeztetést váltana); a `-O2` optimalizál; a `-Iinclude` megmondja, hol keresse a fejléceket.

A két utolsó kapcsoló, a `-MMD -MP`, egy kényelmi finomság: ezekkel a fordító **fejléc-függőségi fájlokat** generál, így ha módosítasz egy fejlécet (mondjuk a `protocol.hpp`-t), a `make` pontosan azokat a forrásfájlokat fordítja újra, amik tényleg függnek tőle — nem többet, nem kevesebbet. A `LIB_OBJ` a közös könyvtár-objektumokat fogja össze (minden modul a két belépési pont nélkül), amiket aztán a fő program *és* a teszt is használ — így a tesztelt kód *pontosan ugyanaz*, mint ami a programban fut, nem egy külön másolat. A `%.o: %.cpp` egy **minta-szabály** (minden `.cpp`-ből csinálj `.o`-t), a `.PHONY` pedig jelzi, hogy a `run`, `test`, `clean` stb. nem fájlnevek, hanem parancsok. Tiszta, hagyományos Makefile, ami pontosan azt csinálja, amit egy beágyazott projektnél vársz.

Ezzel a kód kész, tesztelt és bizonyítottan robusztus. Hátravan még két dolog: végiggondolni, mit *nem* csináltunk (és miért), és összefoglalni, hogyan futtasd és hogyan bizonyítsd, hogy működik.

---

## 14. Tudatos döntések — amit szándékosan *nem* csináltunk

Egy jó beágyazott kódot legalább annyira meghatároz, amit *kihagy*, mint amit megír. Ez a fejezet azokat a döntéseket gyűjti össze, ahol a kód tudatosan a nehezebb, de helyesebb utat választotta — és pont ezek a döntések azok, amiket egy C3S-interjún érdemes elmondani, mert mérnöki ítélőképességet mutatnak.

**Nem másoltunk struktúrát a vezetékre.** A legkézenfekvőbb (és leghibásabb) megoldás az lett volna, hogy a `Telemetry`-t egy `memcpy`-vel a kimenő pufferbe öntjük. Ehelyett mezőnként szerializálunk. Az ok kettős: a fordító kitöltőbájtokat szúrhat a mezők közé (padding), és a memóriabeli bájtsorrend CPU-függő (az x86 little-endian). Egy `memcpy`-zett struktúra tehát ismeretlen szemetet és fordított számokat küldene egy másik hardvernek. Érdemes tudni, hogy a *korai terv* még egy `__attribute__((packed))` „összepréselt" struktúrát vázolt — a végleges kód ezt szándékosan elhagyta a mezőnkénti, explicit big-endian szerializáció javára, mert az teljesen fordító- és CPU-független. Ez az a hiba, ami egyszer megtanít, miért nem szabad struktúrát a vezetékre másolni.

**Nem használtunk `packed` struktúrákat.** A `packed` attribútum (ami eltünteti a paddinget) csábító, de törékeny: igazítatlan memóriahozzáférést okozhat, ami egyes architektúrákon lassú vagy egyenesen hibás. A mezőnkénti szerializáció ezt a kérdést megkerüli — a memóriabeli elrendezés teljesen független a vezetékbeli formátumtól.

**Nem allokáltunk dinamikusan a vevőoldalon.** A dekóderben nincs `new`, nincs `std::vector`, ami nőne — minden puffer fix méretű tagváltozó, a legrosszabb esetre méretezve. Egy repülő rendszerben a heap töredezettsége és a foglalás kiszámíthatatlan ideje valódi kockázat; sok űr- és repülési kódolási szabvány egyenesen tiltja a futásidejű allokációt. A fix pufferek determinisztikus memóriahasználatot adnak.

**Nem bíztunk a bemenetben.** A fejléc hosszmezőjét soha nem használjuk puffer méretezésére; a puffer eleve a maximumra (`kMaxPayloadSize`) van méretezve, és minden ennél nagyobbat hazudó fejlécet elvetünk. Ez a „soha ne bízz a bemenetben" elv védi ki a puffer-túlcsordulást — pont azt a hibaosztályt, ami repülő szoftverben katasztrófa.

**Nem blokkoltunk.** A `feed()` egyetlen bájtot dolgoz fel és azonnal visszatér; soha nem vár, nem olvas, nem alszik. Ettől használható lenne *ugyanez a kód* egy valódi UART-megszakításkezelőben, ahol minden mikroszekundum számít. A ciklus és az időzítés a `main`-ben, a feldolgozón *kívül* van.

**Vállaltunk egy tudatos kompromisszumot.** A folyamatos szinkrondetektálás elvileg téves újraindítást okozhat, ha egy payload véletlenül tartalmazza a 4 bájtos szinkronmintát. Ennek esélye egy 32 bites markernél nagyjából 1 a 4 milliárdhoz bájtpozíciónként — ezen a linken elhanyagolható —, és a kód ezt *őszintén dokumentálja* a kommentben, ahelyett hogy elhallgatná. Éles rendszerek ezt is kiküszöbölik (fix keretpozíciók, bájt-tömés), de itt a bonyolítás nem érné meg. A jó mérnöki munka része az is, hogy ismered és kimondod a megoldásod határait.

**Tudatosan a robusztusabb dekódert választottuk.** A fejlesztés közben a dekóder először a „naiv v1" változat volt, ami csak vadászat közben figyelt szinkronszóra. Ez megbukott a csonkítás-teszten (egy félkész keret elnyelte a következő SYNC-jét). A döntés az volt, hogy átállunk a „profi" változatra: folyamatos szinkrondetektálás minden állapotban. Ez nemcsak helyesebb, hanem elegánsabb is (a SYNC-kezelés egyetlen helyre kerül), és ettől lett a robusztusság-teszt egyértelmű. Ez a fajta „a teszt mutatott egy hiányosságot, és a tervet javítottuk" történet pont az, amit egy interjún érdemes elmondani.

---

## 15. Futtatás, bizonyíték, továbblépés

### Hogyan futtasd

A projekt egyetlen `make` paranccsal lefordul, függőség nélkül:

```
make            # lefordít mindent (a programot és a teszteket)
make test       # a teljes tesztcsomag  →  cél: "ALL 42 CHECKS PASSED"
make run        # élő dashboard, önjáró link-romlással (Ctrl-C kilép)
make snapshot   # egyetlen állókép (jó képernyőfotóhoz)
```

A `make run` adja az élő élményt: a dashboard másodpercenként többször frissül, a telemetria mozog, és a link a `ber_sweep` menetrend szerint romlik-javul, miközben látod a CRC-hibaszámláló reakcióját.

### A bizonyíték, hogy működik

A legmeggyőzőbb demonstráció egy **kontraszt**: futtasd le ugyanazt tiszta és romlott linken, és hasonlítsd össze a dekóder statisztikáit.

```
./satlink --once --ber 0      # tiszta link
./satlink --once --ber 1e-3   # romlott link
```

A két futás eredménye (ugyanazzal a maggal, hogy a telemetria azonos legyen):

| | Tiszta link (`--ber 0`) | Romlott link (`--ber 1e-3`) |
|---|---|---|
| Érvényes keret | 64 | 51 |
| CRC-hiba | 0 | 9 |
| Eldobott (lyuk) | 0 | 12 |
| Keret-hozam | 100,0 % | 85,0 % |
| Megbillentett bit | 0 / 11776 | 15 / 11776 |

Ez a kontraszt *maga a bizonyíték*. Tiszta linken minden keret átmegy, nulla CRC-hibával, 100%-os hozammal. Romlott linken a CRC **pontosan a rossz kereteket fogja ki** — kilenc keret elbukik, a hozam 85%-ra esik —, miközben a jó keretek továbbra is rendben dekódolódnak. Nincs kérdés, működik-e: akkor jó, ha az ép telemetria átmegy és a rontott elakad a CRC-n. Pontosan ez történik.

### Mi jött vissza neked nyelvileg

Ha végigolvastad, közben szépen lassan visszatért egy adag modern C++, amit egy 2005-ös kód nem ismert. Érdemes egy bekezdésben összeszedni, mit tartasz most a kezedben.

A **`constexpr`** a `#define` és a régi `const` modern utódja: típusos, hatókörbe zárt, fordítási idejű konstans, amivel számolni is lehet. Az **`enum class`** a típusbiztos, névteret nem szennyező felsorolás, opcionálisan rögzített tárolási mérettel (`: uint8_t`). Az **alapértelmezett tagértékek** (`uint16_t seq_ = 0;`) garantálják, hogy egy objektum soha ne szülessen szeméttel. A **kapcsos inicializálás** (`{0.0, 1.0}`, `Telemetry last_{};`) az egységes, egyértelmű inicializálási szintaxis. Az **`auto`** a fordítóra bízza a típust ott, ahol amúgy is egyértelmű (vagy le sem írható, mint a lambdáknál).

A **lambdák** (`[&](Row& r){ ... }`) helyben definiált, névtelen függvények, amik a `[&]` capture-rel hozzáférnek a környező változókhoz — ezek tették a dashboard rajzolását és az argumentumkezelőt tömörré. A **`<random>`** kettéválasztja a *motort* (`std::mt19937`, a kiváló minőségű, determinisztikus generátor) és az *eloszlást* (`std::uniform_real_distribution`), és a magozással reprodukálható futásokat ad. A **`<chrono>` és `<thread>`** típusos időtartamokkal (`std::chrono::milliseconds`) és hordozható altatással (`std::this_thread::sleep_for`) váltotta ki a platformfüggő `Sleep`/`usleep`-et.

És jött egy csokor apróság, amik együtt teszik „modernné" a kódot: az **anonim névtér** a fájlszintű `static` helyett; a **nevesített castok** (`static_cast`, `reinterpret_cast`) a C-cast helyett, kereshetően és szándékot kifejezve; a **`#pragma once`** az include-őr helyett; a **`volatile std::sig_atomic_t`** a szignálkezelés helyes idiómája; a **számjegy-elválasztó** (`1'000'000`) az olvashatóságért; és a makró-fogások (a `do/while(0)` egyutasításos csomagolás, a `#cond` stringizálás, a `__FILE__`/`__LINE__` forráshely) a kis tesztkeretben. Ezen felül visszatért két örök C++-csapda is, amik a magyarázatok közben előkerültek: az **integer promotion** (a keskeny egészek `int`-té léptetése aritmetika előtt — ezért a sok `static_cast<uint16_t>` az eltolások után), és a **kettes komplemens** trükkje (egy előjeles szám ugyanaz a bitminta, mint az előjel nélküli párja, ezért lehet a `put_i16`-ot a `put_u16`-ra építeni).

### Hol kezdd a kód olvasását

Ha most leülsz a kód mellé, a `decoder.cpp` `feed()` függvényével kezdd — az a mag, és a 9. fejezet pont azt járja körül. Onnan kifelé minden réteg már ismerős lesz: lefelé a CRC és a szerializáció, amikre támaszkodik, fölfelé a `main`, ami táplálja, és a dashboard, ami láthatóvá teszi. A teljes kód végig változatlanul fordul, tiszta fordítással, és mind a 42 teszt zöld.

### Továbblépés

Ha a projektet tovább akarod vinni — akár a portfólióban, akár tanulásból —, három természetes irány adódik, és mindegyik egy újabb beágyazott készséget gyakoroltat. Az első: a fő ciklust egy **valódi soros portra** (`/dev/ttyUSB0`) kötni, *ugyanazzal a dekóderrel* — így a szimulált linkből igazi hardveres link lesz, és a dekóder egy sora sem változik (ez bizonyítja, hogy a `feed()` tényleg hardver-kész). A második: **Reed–Solomon hibajavítás** hozzáadása, hogy a vevő ne csak *detektálni*, hanem *javítani* is tudja a hibákat — ez egy lépés a valódi űr-protokollok felé, ahol az újraküldés nem opció. A harmadik: egy **második keretezés** (COBS vagy bájt-tömés) megvalósítása összehasonlításként, hogy lásd, hogyan lehet a szinkronszó-ütközés problémáját teljesen kiküszöbölni.

De ezek nélkül is teljes a kép: van egy működő, tesztelt, dokumentált projekted, ami egy űrszabványra épül, és pontosan azokat a készségeket demonstrálja, amiket a C3S beágyazott pozíciója kér. És ami a legfontosabb — most már érted is, minden sorát.
