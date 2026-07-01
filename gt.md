# SIM7080G modem lap (`modem.kicad_sch`) – teljes bekötési útmutató

Ez a lap most üres, csak három interfész-címke van rajta. Ez a doksi végigvezet, mit rakj rá és **pontosan mit hova köss**, hogy csak össze kelljen húzogatnod a huzalokat KiCad-ben.

Fő forrás: *SIM7080G Hardware Design V1.05* (a modul lábai a Table 4/6 szerint, a referencia-kapcsolások a Fig 5/7/12/16/17/25/28 szerint).

Rövidítés a doksiban: **U40** = maga a SIM7080G modul. A lábszámok mindig a SIM7080G lábai.

---

## 0. Mit hoz be az ESP oldal (a gyökér, `portable.kicad_sch`)

Ezeket **nem neked kell megcsinálni**, ezek már megvannak a gyökérben. Csak tudnod kell, mi micsoda és milyen feszültségszintű, mert ehhez kötsz:

| Bejövő jel (címke a lapon) | Irány | Szint | Mi ez / mihez kötöd a modem lapon |
|---|---|---|---|
| `MODEM.NRST` | ESP → modem | **3,3 V**, aktív **MAGAS** | Bekapcsoló/reset impulzus. Egy N-MOS-t hajt, ami a PWRKEY-t földre húzza. (ESP alapból alacsony = modem ki.) |
| `MODEM.NINT` | modem → ESP | 3,3 V (a szintillesztő adja) | A modul RI lába. Alap magas, URC/SMS-nél 120 ms alacsony impulzus. |
| `MODEM.TXD` | ESP → modem | 3,3 V (soros R az ESP-nél) | Az **ESP adóvonala** → a modul **RXD** lába (2). |
| `MODEM.RXD` | modem → ESP | 3,3 V | A modul **TXD** lába (1) → az **ESP vevővonala**. |
| `MODEM.RTS` | ESP → modem | 3,3 V (soros R az ESP-nél) | Az **ESP RTS** → a modul **RTS** lába (3). |
| `MODEM.CTS` | modem → ESP | 3,3 V | A modul **CTS** lába (4) → az **ESP CTS**. |
| `MODEM_PROG.USB_D+` | ↔ | USB HS | Az USB mux modem-csatornája → a modul **USB_DP** (25). |
| `MODEM_PROG.USB_D-` | ↔ | USB HS | Az USB mux modem-csatornája → a modul **USB_DM** (26). |
| `MODEM_PROG.EN` | ESP/jumper → modem | **3,3 V**, aktív **MAGAS** | „Programozó mód” engedélyező. A JP1 jumperrel behúzva MAGAS. Ez kapuzza a USB_BOOT (és opcionálisan a VBUS) felhúzását. |
| `MODEM_ANT` | modem → gyökér | 50 Ω RF | A leillesztett antennajel, ami a gyökérben egy **U.FL** csatlakozóra megy. |

> **Fontos, hogy tudd:** a gyökérben **nincs UART szintillesztő**, ezért az összes `MODEM.*` net **3,3 V-os**. A 3,3 V ↔ 1,8 V átalakítást **ezen a lapon** csinálod (lásd 4. blokk). A `MODEM_TX_R` és `MODEM_RTS_R` soros ellenállások már megvannak az ESP oldalon – **ne** duplázd őket.

---

## 1. Honnan vedd a SIM7080G szimbólumot + footprintet

Nem kell kézzel megrajzolnod a 77 lábat. Kész, KiCad-be exportálható forrás:

1. **SnapMagic Search (régen SnapEDA)** – ingyenes, KiCad export (`.kicad_sym` + `.kicad_mod` + 3D):
   `https://www.snapeda.com/parts/SIM7080G/SIMCom/view-part/`
   (a konkrét cikkszámos verzió: `SIM7080G S2-108HB-Z30CX`)
2. **Component Search Engine (SamacSys)** – szintén ingyenes, KiCad plugin/loader:
   `https://componentsearchengine.com/part-view/SIM7080G/SIMCOM`
3. **TechStudio SIM7080 dev board** – nyílt KiCad (v5) referenciaterv, kimásolhatod belőle a szimbólumot/footprintet, és jó a bekötés ellenőrzésére is:
   `https://github.com/techstudio-design/sim7080_dev_board`
   (a memóriádban meglévő LilyGO T-SIM7080G is jó kereszt-ellenőrzés)

> ⚠️ **Mielőtt gyártatsz:** a letöltött footprintet **vesd össze az adatlap Fig 4-gyel** (Footprint recommendation). A 77 lábas LGA/kastélyos modulnál a third-party footprint pad-méretei néha eltérnek. A pad-raszter és a „Module Area” alatti nagy GND stimmeljen.

---

## 2. Alkatrészlista a modem laphoz

A refdes-eket KiCad úgyis újraszámozza; a **funkció + érték** a lényeg. (A gyökérben már foglalt jelölések elkerülésére 40-es tartományt használok.)

| Refdes (javasolt) | Érték / típus | Mire kell |
|---|---|---|
| U40 | SIM7080G | a modul |
| U41 | **TXB0108** (pl. TXB0108PWR) | UART + RI szintillesztő (1,8↔3,3 V) |
| J40 | nanoSIM foglalat (push-push/hinged) | SIM |
| FB40 | ferritgyöngy (pl. 600 Ω @100 MHz, ≥1 A) | VBAT szűrés |
| SW40 | nyomógomb (SPST) | PWRKEY kézi be/ki |
| Q40 | **BSS138** (N-MOS) | PWRKEY földre húzó |
| Q41 | **BSS138** (N-MOS) | USB_BOOT kapuzás – engedélyező |
| Q42 | **DMG2305UX** (P-MOS, alacsony Vgs(th)) | USB_BOOT felhúzása VDD_EXT-re |
| Q43 | **MMBT3904** (NPN) | NETLIGHT LED hajtás |
| Q44, Q45 *(opcionális)* | DMG2305UX (P) + BSS138 (N) | VBUS kapuzás (ha akarod, hogy a modem tudjon aludni) |
| LED40 | piros LED (0603) | hálózati státusz |
| D40 | ESD/TVS (pl. ESD5Vx, kis kapacitású) | PWRKEY ESD |
| D41 | TVS (kis kapacitású) | USB_BOOT ESD |
| D42 | **kis kapacitású RF TVS** (pl. BLE5V0CR05UB 0,05 pF, vagy WE05DGCF-B 0,3 pF) | antenna ESD |
| D43 | SIM ESD tömb, **≤15 pF** (pl. ESDA6V1W5, SMF15C) | SIM ESD |
| C40, C41 | 100 µF (low-ESR) | VBAT bulk |
| C42 | 1 µF | VBAT |
| C43 | 100 nF | VBAT |
| C44 | 100 nF | VDD_EXT dekap |
| C45 | 100 nF | TXB VCCA (1,8 V) dekap |
| C46 | 100 nF | TXB VCCB (3,3 V) dekap |
| C47 | 100 nF | SIM_VDD (foglalathoz közel) |
| C48 | 100 nF (+ opc. 4,7 µF) | USB_VBUS szűrés |
| C49 | 100 pF | antenna DC-block (C3) |
| C50, C51 | DNP (hely) | antenna illesztő C1/C2 (érték az antennától) |
| R40 | 1 kΩ | PWRKEY soros |
| R41 | 100 kΩ | Q40 gate pulldown |
| R42 | 10 kΩ | USB_BOOT soros (Q42 → USB_BOOT) |
| R43 | 100 kΩ | Q41 gate pulldown |
| R44 | 100 kΩ | Q42 gate felhúzás (VDD_EXT-re) |
| R45 | 22 Ω | SIM_CLK soros |
| R46 | 22 Ω | SIM_RST soros |
| R47 | 22 Ω | SIM_DATA soros |
| R48 | 0 Ω | antenna R1 (soros illesztő, alap 0 Ω) |
| R49 | 4,7 kΩ | NETLIGHT → Q43 bázis |
| R50 | 47 kΩ | Q43 bázis pulldown |
| R51 | ~470–510 Ω | NETLIGHT LED áramkorlát |
| R52,R53,R54 *(opc.)* | 100 kΩ | felhúzás a `MODEM.RXD/CTS/NINT`-re (idle-magas, ha a modem ki van kapcsolva) |

Táp-szimbólumok, amiket a lapra teszel (globálisak): **`+3V3`**, **`+5V`**, **`GND`**.

---

## 3. FŐ TÁBLÁZAT – a SIM7080G minden lába → mihez kötöd

Ez a „csak kösd össze” lényeg. A részletes al-kapcsolásokat a 4–9. blokk írja le.

| Láb | Név | Kötés |
|---|---|---|
| 34, 35 | VBAT | `+3V3` → **FB40** → VBAT; a lábnál C40‖C41(100µF)‖C42(1µF)‖C43(100nF)→GND; **D40-típusú TVS**→GND |
| 8,13,19,21,27,30,31,33,36,37,45,63,66,67,69,70,71,72,73,74,75,76,77 + központi pad | GND | mind **`GND`** |
| 40 | VDD_EXT | net `VDD_EXT`; **C44 100nF**→GND; **test point**; ez adja a TXB VCCA-t és a USB_BOOT felhúzást |
| 39 | PWRKEY | ← **R40 1kΩ** ← Q40 drain; **SW40** a PWRKEY↔GND közé; **D40** ESD→GND *(4. blokk)* |
| 1 | UART1_TXD (modul kimenet) | → TXB **A2** *(5. blokk)* |
| 2 | UART1_RXD (modul bemenet) | → TXB **A1** |
| 3 | UART1_RTS (modul bemenet) | → TXB **A3** |
| 4 | UART1_CTS (modul kimenet) | → TXB **A4** |
| 7 | UART1_RI (modul kimenet) | → TXB **A5** |
| 5 | UART1_DCD | **nyitva** (No-Connect) |
| 6 | UART1_DTR | **nyitva** (No-Connect) |
| 42 | STATUS | **nyitva** (opc. saját LED) |
| 41 | NETLIGHT | → **R49 4,7kΩ** → Q43 bázis *(9. blokk)* |
| 20 | USB_BOOT | ← **R42 10kΩ** ← Q42 drain; **D41** TVS→GND; **test point** *(6. blokk)* |
| 24 | USB_VBUS | ← `+5V` (FB/0 Ω + C48) **VAGY** kapuzott 5 V *(6. blokk)* |
| 25 | USB_DP | ↔ `MODEM_PROG.USB_D+` (90 Ω diff, rövid) |
| 26 | USB_DM | ↔ `MODEM_PROG.USB_D-` (90 Ω diff, rövid) |
| 15 | SIM_DATA | → **R47 22Ω** → J40 I/O *(7. blokk)* (külső pull-up **NEM** kell) |
| 16 | SIM_CLK | → **R45 22Ω** → J40 CLK |
| 17 | SIM_RST | → **R46 22Ω** → J40 RST |
| 18 | SIM_VDD | → J40 VCC; **C47 100nF** a foglalatnál →GND |
| 32 | RF_ANT | → π-illesztő + C3 + TVS → `MODEM_ANT` *(8. blokk)* |
| 68 | GNSS_ANT | **nyitva** (GNSS-t nem használunk) |
| 43, 44 | ANT_CONTROL1/0 | **nyitva** |
| 48,49,50,51 | SPI_CS/MOSI/CLK/MISO | **nyitva** – ⚠️ **49 (SPI_MOSI): TILOS felhúzni!** boot előtt magas → nem indul |
| 64, 65 | I2C_SDA/SCL | **nyitva** |
| 9,10,11,12 | PCM_DIN/DOUT/CLK/SYNC | **nyitva** |
| 38 | ADC | **nyitva** |
| 22, 23 | UART2_TXD/RXD | **nyitva** (debug/boot-log; opc. test point) |
| 61, 62 | UART3_TXD/RXD | **nyitva** |
| 57,58,59,60,14 | GPIO1..4, GPIO5 | **nyitva** |
| 28,29,46,47,52,53,54,55,56 | NC | **nyitva** |

> KiCad-ben a „nyitva” lábakra tedd rá a **No-Connect (X) flag**-et, hogy az ERC ne panaszkodjon. **Kivéve az SPI_MOSI-t (49)** – arra is No-Connect flag mehet, csak fizikailag semmilyen pull ne kerüljön rá.

---

## 4. PWRKEY (bekapcsolás/reset) – `MODEM.NRST`

A PWRKEY belül 1,5 V-ra van húzva; földre kell rántani a be/ki-hez. N-MOS-szal húzod, a gyári Fig 7 szerint.

Kösd így:
- `MODEM.NRST` → **Q40 gate**
- **R41 100 kΩ**: Q40 gate ↔ GND (alap zárt)
- **Q40 source** → GND
- **Q40 drain** → **R40 1 kΩ** → PWRKEY (39)
- **SW40** nyomógomb: PWRKEY (39) ↔ GND
- **D40** ESD/TVS: PWRKEY (39) ↔ GND (a lábhoz közel)

Működés: `MODEM.NRST` **magas** → Q40 nyit → PWRKEY földön → modem be/ki. Gomb megnyomva ugyanez kézzel.

**Firmware-időzítés (adatlap Table 8/9):**
- Bekapcsolás: PWRKEY-t **≥1 s** (max 12,6 s) tartsd alacsonyan.
- Kikapcsolás: **≥1,2 s** alacsony.
- **Ne** tartsd 12,6 s-nél tovább → auto-reset!
- Bekapcsolás után **~1,8 s**-ig ne küldj AT-t (addig áll fel a modem; a STATUS ekkor megy magasra, de azt nem kötöttük ki – időzítéssel vagy AT-válaszra várva detektáld a készenlétet).

---

## 5. UART + RI szintillesztő (TXB0108) – `MODEM.RXD/TXD/RTS/CTS/NINT`

Egyetlen **TXB0108** (8 csatorna, push-pull, auto-irány – ezt használja az adatlap Fig 12 is). **5 csatornát** használsz. Az **A-oldal = 1,8 V (modul), B-oldal = 3,3 V (ESP netek)**.

Táp/engedélyezés:
- **VCCA → `VDD_EXT`** (1,8 V) + **C45 100nF**→GND
- **VCCB → `+3V3`** + **C46 100nF**→GND
- **OE → `VDD_EXT`** (folyamatosan engedélyezve)
- **GND → GND**

Csatornák (a párosítás fontos!):

| TXB A (1,8 V) | ← köt → | modul láb | TXB B (3,3 V) | ← köt → | ESP net |
|---|---|---|---|---|---|
| A1 | – | 2 (RXD) | B1 | – | `MODEM.TXD` |
| A2 | – | 1 (TXD) | B2 | – | `MODEM.RXD` |
| A3 | – | 3 (RTS) | B3 | – | `MODEM.RTS` |
| A4 | – | 4 (CTS) | B4 | – | `MODEM.CTS` |
| A5 | – | 7 (RI) | B5 | – | `MODEM.NINT` |
| A6–A8, B6–B8 | | | | | **nyitva** |

Megjegyzések:
- RTS/CTS **egyenesen** (straight) van kötve, ahogy a Fig 10 „full modem” ábra mutatja. Ha a flow control fordítva viselkedne, firmware-ben (`esp_modem` DTE config) fordítható.
- **Ne** tegyél erős pull-upot a TXB vonalaira (a belső ~4 kΩ-t vernék). Ha idle-magas szintet akarsz, amikor a modem KI van (VDD_EXT=0), akkor a B-oldalra opcionálisan **R52/R53/R54 100 kΩ → `+3V3`** a `MODEM.RXD`, `MODEM.CTS`, `MODEM.NINT` netekre (vagy hagyd az ESP belső felhúzására).

---

## 6. USB útvonal + USB_BOOT + VBUS – `MODEM_PROG.*`

### USB adat
- `MODEM_PROG.USB_D+` → **USB_DP (25)**
- `MODEM_PROG.USB_D-` → **USB_DM (26)**
- 90 Ω differenciál, rövid, párban. **Extra USB TVS itt nem kell** – a gyökérbeli PI3USB221 muxban van beépített ESD, ami a modem D+/D- oldalát is védi.

### USB_BOOT kapuzás (KÖTELEZŐ így!)
A USB_BOOT-ot **csak programozáskor** szabad VDD_EXT-re húzni (`EN` magas), normál boot-nál nyitva (adatlap Fig 16: „DO NOT PULL UP DURING NORMAL POWER UP”). Ezért 1 N-MOS + 1 P-MOS kapu:

- **Q41 (BSS138)**: gate = `MODEM_PROG.EN`; **R43 100 kΩ** gate↔GND; source→GND; drain → node `BOOT_G`
- **Q42 (DMG2305UX, P-MOS)**: source = `VDD_EXT`; gate = `BOOT_G`; **R44 100 kΩ** gate↔source (VDD_EXT); drain → **R42 10 kΩ** → USB_BOOT (20)
- **D41** TVS: USB_BOOT (20) ↔ GND
- **Test point** a USB_BOOT (20)-on

Működés: `EN` magas → Q41 nyit → `BOOT_G`=GND → Q42 nyit → VDD_EXT a 10 kΩ-on át felhúzza USB_BOOT-ot → forced download. `EN` alacsony → Q42 zár → USB_BOOT nyitva → normál boot.

### USB_VBUS – válaszd az egyiket

**A) Egyszerű (alapértelmezett, USB-tápos eszközhöz jó):** USB_VBUS (24) ← `+5V` egy kis **FB vagy 0 Ω**-on át, **C48 100nF (+ opc. 4,7 µF)**→GND.
→ Előny: kevés alkatrész. Hátrány: a modem folyton „USB bedugva” állapotot lát, ezért **nem fog aludni (PSM/sleep)**. USB-tápos pultnál ez rendben van.

**B) Kapuzott (ha akarod a modem alvását):** `+5V` → P-MOS (**Q44 DMG2305UX**, source=+5V, drain=USB_VBUS(24), gate felhúzva 100 kΩ-mal +5V-ra), amit a `MODEM_PROG.EN` egy N-MOS-on (**Q45 BSS138**) keresztül kapcsol (gate=EN, drain=Q44 gate, source=GND). USB_VBUS(24)↔GND egy 100 kΩ, hogy KI állapotban tiszta 0 V legyen. + C48.
→ Így a modem USB-je csak programozáskor él, egyébként a modem tud PSM-be menni.

---

## 7. SIM foglalat – `J40` (nanoSIM, 1,8 V)

A SIM7080G **csak 1,8 V-os SIM**-et támogat. Adatlap Fig 17 / 3.5.2:

- **SIM_VDD (18)** → J40 **VCC**; **C47 100 nF** VCC↔GND, a foglalathoz közel
- **SIM_CLK (16)** → **R45 22 Ω** → J40 **CLK**
- **SIM_RST (17)** → **R46 22 Ω** → J40 **RST**
- **SIM_DATA (15)** → **R47 22 Ω** → J40 **I/O** — **külső pull-up NEM kell** (belül 20 kΩ-mal VDD-re húzva)
- J40 **GND** → GND
- **D43** ESD-tömb (**≤15 pF** parazita kapacitás) a SIM vonalakra, a foglalathoz közel

Layout: J40 legyen **távol az antennától** és RF vonalaktól; a SIM_CLK-t GND-vel guard-old; a SIM vonalak ne legyenek túl hosszúak, ne ágazzanak el.

---

## 8. Antenna illesztés – `RF_ANT (32)` → `MODEM_ANT`

A `MODEM_ANT` a gyökérben egy U.FL-re megy. Az **illesztést és az ESD-t a modul mellé** tedd (adatlap Fig 28), 50 Ω-os vonallal:

Sorrendben az RF_ANT (32) lábtól:
1. **C50 (C1)** shunt → GND  *(DNP hely, érték az antennától)*
2. **R48 (R1) 0 Ω** soros
3. **C51 (C2)** shunt → GND  *(DNP hely)*
4. **C49 (C3) 100 pF** soros (DC-block + ESD)
5. → node `ANT` → **`MODEM_ANT`** címke
6. **D42** kis kapacitású TVS: `ANT` ↔ GND

Megjegyzés: az illesztő π-tag (C1/C2) értékeit az antenna gyártója/hangolás adja; most csak a **helyet és a topológiát** hozd létre (R1=0 Ω, C1/C2=DNP, C3=100 pF). A `MODEM_ANT` net legyen **50 Ω, rövid**, jobb/hegyesszög nélkül, sok GND via-val, más gyorsvonaltól távol.

GNSS-t nem használunk → **GNSS_ANT (68) nyitva** (közé GND). Egyszerre GNSS+mobil amúgy sem menne (közös RX).

---

## 9. NETLIGHT LED – `NETLIGHT (41)`

Piros státusz-LED, NPN hajtással (adatlap Fig 25). A villogási minták (Table 14): 64 ms be / 800 ms ki = nincs hálózat; 64 ms/3000 ms = regisztrálva; 64 ms/300 ms = adatforgalom; ki = kikapcsolva/PSM.

- **NETLIGHT (41)** → **R49 4,7 kΩ** → **Q43 bázis**
- **R50 47 kΩ**: Q43 bázis ↔ GND
- **Q43 kollektor** → **LED40 katód**; **LED40 anód** → **R51 ~470–510 Ω** → **VBAT** (a modul VBAT-járól, ahogy a gyári ábra)
- **Q43 emitter** → GND

Működés: NETLIGHT magas → Q43 nyit → LED világít.

---

## 10. Nettábla (a fontos csomópontok gyors ellenőrzéshez)

| Net | Mi lóg rajta |
|---|---|
| `VDD_EXT` | U40/40, C44, TXB VCCA, TXB OE, Q42 source, R44 |
| `MODEM.NRST` | Q40 gate (R41-gyel GND-re) |
| `MODEM.NINT` | TXB B5 |
| `MODEM.TXD` | TXB B1 |
| `MODEM.RXD` | TXB B2 |
| `MODEM.RTS` | TXB B3 |
| `MODEM.CTS` | TXB B4 |
| `MODEM_PROG.EN` | Q41 gate (R43-mal GND-re) *(és opc. Q45 gate)* |
| `MODEM_PROG.USB_D+` | U40/25 |
| `MODEM_PROG.USB_D-` | U40/26 |
| `BOOT_G` | Q41 drain, Q42 gate, R44 |
| `MODEM_ANT` | C49 után, D42 |
| `+5V` | USB_VBUS forrás (FB/0Ω vagy Q44) |
| `+3V3` | FB40 (→VBAT), TXB VCCB, C46 |

---

## 11. Layout / gyártási emlékeztetők

- **VBAT**: a bulk (C40/C41/C42/C43) + FB40 + TVS a **lábhoz a lehető legközelebb**; VBAT trace **≥1 mm** széles, rövid, sok GND via. Adáskor 0,5 A tüske → ha van hely, inkább 3× 100 µF.
- A modul köré **≥3 mm keepout** (Fig 4 / 2.3), hogy javítható legyen.
- **RF vonal** 50 Ω, rövid; **USB D+/D-** 90 Ω diff, rövid, párban; **SIM** távol az antennától.
- **Test point** a `VDD_EXT`-re és a `USB_BOOT`-ra (az adatlap kifejezetten kéri).
- A footprintet **ellenőrizd a Fig 4-hez** gyártás előtt.

---

## 12. Beültetési checklist (pipáld végig KiCad-ben)

- [ ] SIM7080G szimbólum+footprint importálva, ERC-hez rendben
- [ ] Minden GND láb (a központi pad is) → `GND`
- [ ] VBAT: `+3V3` → FB40 → VBAT, bulk + TVS a lábnál
- [ ] VDD_EXT: C44 + test point
- [ ] PWRKEY: Q40 + R40(1k) + R41(100k) + SW40 + D40
- [ ] TXB0108: VCCA=VDD_EXT, VCCB=+3V3, OE=VDD_EXT, C45/C46, 5 csatorna bekötve (A↔modul, B↔MODEM.\*)
- [ ] SPI_MOSI (49) **semmilyen pull-up nélkül**, No-Connect flag
- [ ] USB: D+/D- a MODEM_PROG-hoz; USB_BOOT kapu (Q41+Q42+R42+R43+R44+D41) + test point
- [ ] USB_VBUS: A) konstans `+5V`+C48 **vagy** B) kapuzott (Q44+Q45)
- [ ] SIM: J40 + 3× 22Ω + C47 + D43, DATA-n nincs külső pull-up
- [ ] Antenna: C50(DNP)/R48(0Ω)/C51(DNP)/C49(100pF) + D42 → MODEM_ANT
- [ ] GNSS_ANT (68) nyitva
- [ ] NETLIGHT: R49/R50/Q43/LED40/R51 VBAT-ról
- [ ] Minden nem használt láb No-Connect flaggel
- [ ] ERC lefut hiba nélkül; a három hierarchikus címke (`MODEM`, `MODEM_PROG`, `MODEM_ANT`) illeszkedik a gyökér lap pin-jeihez

---

## 13. Amit érdemes fejben tartani (kompromisszumok)

- **3,3 V-os VBAT:** a modul 2,7–4,8 V-ot vár, tipikus 3,8 V. A 3,3 V a tartomány alsó vége, és a 0,5 A adási tüskék IR-esése levihet 3,0 V alá. Két dolgon múlik, hogy stabil-e: **elég nagy, alacsony ESR-ű bulk közvetlenül a VBAT-nál**, és hogy a **3,3 V-os LDO (AMS1117/LDI1117) elbírja-e az 500 mA csúcsokat**. Ha mérésnél a VBAT adáskor 3,0 V alá esik, több/jobb bulk vagy dedikált buck kell (adatlap Fig 6).
- **`EN` polaritás:** a gyökérben ellenőrizve – **+3V3 → 100 Ω (R77) → JP1 → EN → 10 kΩ (R41) → GND**, tehát `EN` **aktív magas** (jumper behúzva = programozó mód). A fenti kapuk ehhez vannak méretezve.
- **RTS/CTS:** straight bekötés (Fig 10). Ha az `esp_modem` flow control fordítva viselkedne, szoftverből vagy 0 Ω-cserével cserélhető.
- **STATUS nincs kivezetve** (a `MODEM` busz csak NRST-et és NINT-et hoz) – a készenlétet firmware-ből (AT-válasz + időzítés) detektáld.
