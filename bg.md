# SIM7080G modem lap (`modem.kicad_sch`) – teljes bekötési útmutató (v2)

**Változás v1-hez képest (Discord-döntés alapján):** a TXB0108 szintillesztő IC **kiesik**, helyette **vonalanként 1 db BSS138** N-MOS, kapcsolóüzemben (gate = bemenet, source = GND, drain = kimenet, felhúzó a gate-en és a drainen). Így **egyetlen félvezető-típus (BSS138) fedi le** a szintillesztést, a PWRKEY-t, a USB_BOOT-kapuzást és a NETLIGHT LED-hajtást is – nincs új alkatrésztípus a tervben. Az ára: **a fokozat invertál**, ezért ⚠️ **a firmware-ben a UART mind a 4 vonalát invertálni kell** (részletek az 5. és 14.4 pontban).

Fő forrás: *SIM7080G Hardware Design V1.05* (lábak: Table 4/6; referencia-kapcsolások: Fig 5/7/13/16/17/25/28). A részletes „miért így + hol van az adatlapban” rész a **14. fejezet**.

Rövidítés: **U40** = a SIM7080G modul. A lábszámok mindig a SIM7080G lábai. A refdes-ek javaslatok, KiCad úgyis újraszámoz.

---

## 0. Mit hoz be az ESP oldal (a gyökér, `portable.kicad_sch`)

Ezeket **nem neked kell megcsinálni**, már megvannak a gyökérben. Csak tudd, mi micsoda, milyen szintű – és mostantól azt is, hogy **a UART-jelek a FET-fokozat miatt fizikailag invertáltak** lesznek az ESP lábain:

| Bejövő jel (címke a lapon) | Irány | Szint | Mi ez / mihez kötöd a modem lapon |
|---|---|---|---|
| `MODEM.NRST` | ESP → modem | 3,3 V, aktív **MAGAS** | Bekapcsoló/reset impulzus → Q40 gate (PWRKEY-lehúzó). Nem invertált értelmű: magas = PWRKEY földön. |
| `MODEM.NINT` | modem → ESP | 3,3 V, **invertált!** | A modul RI lába a Q54 fokozaton át. RI esemény = az ESP lábon **felfutó él** (alap alacsony). |
| `MODEM.TXD` | ESP → modem | 3,3 V, **invertált!** | ESP adóvonal → **Q50 gate** → modul RXD (2). Fw: `TXD_INV`. |
| `MODEM.RXD` | modem → ESP | 3,3 V, **invertált!** | Modul TXD (1) → **Q52** → ESP vevővonal. Fw: `RXD_INV`. |
| `MODEM.RTS` | ESP → modem | 3,3 V, **invertált!** | ESP RTS → **Q51 gate** → modul RTS (3). Fw: `RTS_INV`. |
| `MODEM.CTS` | modem → ESP | 3,3 V, **invertált!** | Modul CTS (4) → **Q53** → ESP CTS. Fw: `CTS_INV`. |
| `MODEM_PROG.USB_D+` | ↔ | USB HS | USB mux modem-csatorna → modul USB_DP (25). |
| `MODEM_PROG.USB_D-` | ↔ | USB HS | USB mux modem-csatorna → modul USB_DM (26). |
| `MODEM_PROG.EN` | jumper → modem | 3,3 V, aktív **MAGAS** | JP1 behúzva = programozó mód. A gyökérben ellenőrizve: `+3V3 →100Ω(R77)→ JP1 → EN →10kΩ(R41)→ GND`. Ez kapuzza a USB_BOOT felhúzását (Q55). |
| `MODEM_ANT` | modem → gyökér | 50 Ω RF | Leillesztett antennajel → a gyökérben U.FL. |

> A `MODEM_TX_R` és `MODEM_RTS_R` soros ellenállások az ESP oldalon megvannak – **ne duplázd**; mostantól FET-gate-eket hajtanak, az teljesen rendben van.

---

## 1. Honnan vedd a SIM7080G szimbólumot + footprintet

1. **SnapMagic Search (SnapEDA)** – ingyenes KiCad export (`.kicad_sym` + `.kicad_mod` + 3D):
   `https://www.snapeda.com/parts/SIM7080G/SIMCom/view-part/` (cikkszámos: `SIM7080G S2-108HB-Z30CX`)
2. **Component Search Engine (SamacSys)**: `https://componentsearchengine.com/part-view/SIM7080G/SIMCOM`
3. **TechStudio SIM7080 dev board** – nyílt KiCad referenciaterv, jó kereszt-ellenőrzésre is:
   `https://github.com/techstudio-design/sim7080_dev_board` (plusz a LilyGO T-SIM7080G)

> ⚠️ Gyártás előtt a letöltött footprintet **vesd össze az adatlap Fig 4-gyel** (Footprint recommendation, V1.05 p20).

---

## 2. Alkatrészlista a modem laphoz

| Refdes (javasolt) | Érték / típus | Mire kell |
|---|---|---|
| U40 | SIM7080G | a modul |
| Q40 | **BSS138** | PWRKEY földre húzó |
| Q50, Q51 | **BSS138** | szintillesztő, ESP→modem irány (TXD, RTS) |
| Q52, Q53, Q54 | **BSS138** | szintillesztő, modem→ESP irány (TXD→RXD, CTS, RI→NINT) |
| Q55 | **BSS138** | USB_BOOT „követő” kapcsoló (VDD_EXT-re húzás EN-nel) |
| Q56 | **BSS138** | NETLIGHT LED hajtás |
| J40 | nanoSIM foglalat | SIM |
| FB40 | ferritgyöngy (600 Ω@100 MHz, ≥1 A) | VBAT szűrés |
| FB41 (v. 0 Ω) | ferritgyöngy | USB_VBUS szűrés |
| SW40 | nyomógomb (SPST) | PWRKEY kézi be/ki |
| LED40 | piros LED (0603) | hálózati státusz |
| D40 | ESD/TVS (kis kapacitású) | PWRKEY ESD |
| D41 | TVS (kis kapacitású) | USB_BOOT ESD |
| D42 | RF TVS, **≤0,3 pF** (BLE5V0CR05UB / WE05DGCF-B) | antenna ESD |
| D43 | SIM ESD tömb, **≤15 pF** (ESDA6V1W5 / SMF15C) | SIM ESD |
| C40, C41 | 100 µF (low-ESR) | VBAT bulk |
| C42 | 1 µF | VBAT |
| C43 | 100 nF | VBAT |
| C44 | 100 nF | VDD_EXT dekap |
| C47 | 100 nF | SIM_VDD (foglalatnál) |
| C48 | 100 nF (+ opc. 4,7 µF) | USB_VBUS |
| C49 | 100 pF | antenna DC-block (C3) |
| C50, C51 | DNP (hely) | antenna illesztő C1/C2 |
| R40 | 1 kΩ | PWRKEY soros |
| R41m | 100 kΩ | Q40 gate lehúzó |
| R42 | 10 kΩ | Q55 source → USB_BOOT soros |
| R45, R46, R47 | 22 Ω | SIM_CLK / RST / DATA soros |
| R48 | 0 Ω | antenna R1 (soros illesztő) |
| R50 | 100 kΩ | Q56 gate lehúzó |
| R51 | ~470–510 Ω | NETLIGHT LED áramkorlát |
| R60, R61 | 100 kΩ | Q50/Q51 **gate-felhúzó → `+3V3`** |
| R62, R63 | **10 kΩ** | Q50/Q51 **drain-felhúzó → `VDD_EXT`** (ez a modem felé menő jel!) |
| R64, R65, R66 | 100 kΩ | Q52/Q53/Q54 **gate-felhúzó → `VDD_EXT`** (⚠️ SOHA nem 3V3-ra – a modul lábán ül!) |
| R67, R68, R69 | **10 kΩ** | Q52/Q53/Q54 **drain-felhúzó → `+3V3`** (ez az ESP felé menő jel!) |

Táp-szimbólumok a lapra: **`+3V3`**, **`+5V`**, **`GND`**.
*(Kiesett v1-hez képest: TXB0108, DMG2305UX P-MOS, MMBT3904, és a hozzájuk tartozó R-ek/C-k.)*

---

## 3. FŐ TÁBLÁZAT – a SIM7080G minden lába → mihez kötöd

| Láb | Név | Kötés |
|---|---|---|
| 34, 35 | VBAT | `+3V3` → **FB40** → VBAT; a lábnál C40‖C41(100µF)‖C42(1µF)‖C43(100nF)→GND; TVS→GND |
| 8,13,19,21,27,30,31,33,36,37,45,63,66,67,69–77 + központi pad | GND | mind **`GND`** |
| 40 | VDD_EXT | net `VDD_EXT`; **C44**→GND; **test point**; innen mennek: R62/R63/R64/R65/R66 felhúzók + Q55 drain |
| 39 | PWRKEY | ← **R40 1kΩ** ← Q40 drain; **SW40** PWRKEY↔GND; **D40** ESD→GND *(4. blokk)* |
| 1 | UART1_TXD (modul ki) | → **Q52 gate** *(5. blokk)* |
| 2 | UART1_RXD (modul be) | ← **Q50 drain** (R62 10k → VDD_EXT) |
| 3 | UART1_RTS (modul be) | ← **Q51 drain** (R63 10k → VDD_EXT) |
| 4 | UART1_CTS (modul ki) | → **Q53 gate** |
| 7 | UART1_RI (modul ki) | → **Q54 gate** |
| 5, 6 | UART1_DCD, DTR | **nyitva** (No-Connect) |
| 42 | STATUS | **nyitva** (opc. saját LED) |
| 41 | NETLIGHT | → **Q56 gate** (R50 100k gate↔GND) *(9. blokk)* |
| 20 | USB_BOOT | ← **R42 10kΩ** ← **Q55 source**; **D41** TVS→GND; **test point** *(6. blokk)* |
| 24 | USB_VBUS | ← `+5V` → **FB41/0Ω** → láb; **C48**→GND *(konstans 5 V a Discord-terv szerint; kapuzott opció a 6. blokkban)* |
| 25 | USB_DP | ↔ `MODEM_PROG.USB_D+` (90 Ω diff, rövid) |
| 26 | USB_DM | ↔ `MODEM_PROG.USB_D-` (90 Ω diff, rövid) |
| 15 | SIM_DATA | → **R47 22Ω** → J40 I/O (külső pull-up **NEM** kell) |
| 16 | SIM_CLK | → **R45 22Ω** → J40 CLK |
| 17 | SIM_RST | → **R46 22Ω** → J40 RST |
| 18 | SIM_VDD | → J40 VCC; **C47** a foglalatnál →GND |
| 32 | RF_ANT | → π-illesztő + C49 + D42 → `MODEM_ANT` *(8. blokk)* |
| 68 | GNSS_ANT | **nyitva** |
| 43, 44 | ANT_CONTROL1/0 | **nyitva** |
| 48,49,50,51 | SPI | **nyitva** – ⚠️ **49 (SPI_MOSI): TILOS felhúzni!** |
| 64, 65 | I2C | **nyitva** |
| 9–12 | PCM | **nyitva** |
| 38 | ADC | **nyitva** |
| 22, 23 | UART2 | **nyitva** (debug/boot-log; opc. test point) |
| 61, 62 | UART3 | **nyitva** |
| 57–60, 14 | GPIO1..5 | **nyitva** |
| 28,29,46,47,52–56 | NC | **nyitva** |

> A „nyitva” lábakra No-Connect (X) flag, hogy az ERC ne szóljon. Az SPI_MOSI (49)-re is No-Connect mehet – csak fizikailag semmilyen pull ne kerüljön rá.

---

## 4. PWRKEY (bekapcsolás/reset) – `MODEM.NRST`

- `MODEM.NRST` → **Q40 gate**; **R41m 100 kΩ** gate↔GND (alap zárt)
- **Q40 source** → GND; **Q40 drain** → **R40 1 kΩ** → PWRKEY (39)
- **SW40**: PWRKEY ↔ GND; **D40** ESD: PWRKEY ↔ GND (lábhoz közel)

Működés: `MODEM.NRST` magas → Q40 nyit → PWRKEY földön. Időzítés: **be: ≥1 s** (max 12,6 s!), **ki: ≥1,2 s**, 12,6 s fölött **auto-reset**; bekapcsolás után **~1,8 s** múlva élnek a portok. *(Részletek + hivatkozás: 14.3.)*

---

## 5. UART + RI szintillesztő – 5× BSS138 (a Discord-recept szerint)

**A recept vonalanként:** gate = bemenet, source = GND, drain = kimenet; felhúzó a gate-re és a drainre. A fokozat **INVERTÁL** – ezt a firmware kompenzálja (lásd a keretes figyelmeztetést).

**ESP → modem irány (2 vonal):**

*Vonal A – adat (ESP küld):*
- **Q50 gate** ← `MODEM.TXD`; **R60 100 kΩ** gate ↔ `+3V3`
- **Q50 source** → GND
- **Q50 drain** → **modul 2 (UART1_RXD)**; **R62 10 kΩ** drain ↔ `VDD_EXT`

*Vonal B – RTS (ESP jelez):*
- **Q51 gate** ← `MODEM.RTS`; **R61 100 kΩ** gate ↔ `+3V3`
- **Q51 source** → GND
- **Q51 drain** → **modul 3 (UART1_RTS)**; **R63 10 kΩ** drain ↔ `VDD_EXT`

**Modem → ESP irány (3 vonal):**

*Vonal C – adat (modem küld):*
- **Q52 gate** ← **modul 1 (UART1_TXD)**; **R64 100 kΩ** gate ↔ `VDD_EXT`
- **Q52 source** → GND
- **Q52 drain** → `MODEM.RXD`; **R67 10 kΩ** drain ↔ `+3V3`

*Vonal D – CTS:*
- **Q53 gate** ← **modul 4 (UART1_CTS)**; **R65 100 kΩ** gate ↔ `VDD_EXT`
- **Q53 source** → GND
- **Q53 drain** → `MODEM.CTS`; **R68 10 kΩ** drain ↔ `+3V3`

*Vonal E – RI:*
- **Q54 gate** ← **modul 7 (UART1_RI)**; **R66 100 kΩ** gate ↔ `VDD_EXT`
- **Q54 source** → GND
- **Q54 drain** → `MODEM.NINT`; **R69 10 kΩ** drain ↔ `+3V3`

> ⚠️ **FIRMWARE-KÖTELEZETTSÉG (ha ez kimarad, semmi nem működik):** az ESP32-C6 UART-ját inverz módba kell tenni mind a 4 vonalon:
> `uart_set_line_inverse(port, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV | UART_SIGNAL_RTS_INV | UART_SIGNAL_CTS_INV);`
> (az `esp_modem` DTE létrehozása után, ugyanarra a portra). A **RI/NINT** is fordított: RI esemény = **felfutó él** az ESP lábon (alap: alacsony).

> ⚠️ **Gate-felhúzó szabály:** a modem-oldali gate-ek (Q52/Q53/Q54) felhúzója **kizárólag `VDD_EXT`-re** mehet – 3,3 V a modul lábán abszolút-maximum-sértés (max 2,1 V, lásd 14.4). Az ESP-oldali gate-ek (Q50/Q51) felhúzója `+3V3`-ra megy, a Discord-recept szerint.

Sebesség: 10 kΩ drain-felhúzókkal **115200 baudon kényelmes** (az `esp_modem` alap/autobaud tartomány úgyis 9600–115200); ha valaha 921600-ra mennétek, a drain-felhúzókat cseréld 4,7 kΩ-ra. *(Számítás: 14.4.)*

---

## 6. USB útvonal + USB_BOOT + VBUS – `MODEM_PROG.*`

### USB adat
- `MODEM_PROG.USB_D+` → **USB_DP (25)**; `MODEM_PROG.USB_D-` → **USB_DM (26)**
- 90 Ω differenciál, rövid, párban. Extra TVS nem kell – a gyökérbeli PI3USB221 mux beépített ESD-je védi ezt az ágat is.

### USB_BOOT kapuzás – 1 db BSS138 „követőként” (Q55)
- **Q55 gate** ← `MODEM_PROG.EN` (a gyökér R41 10 kΩ lehúzója már definiálja, külön lehúzó nem kell)
- **Q55 drain** → `VDD_EXT`
- **Q55 source** → **R42 10 kΩ** → **USB_BOOT (20)**
- **D41** TVS: USB_BOOT ↔ GND; **test point** a lábon

Működés: `EN` alacsony → Q55 zár → USB_BOOT nyitva (belső lehúzó tartja) → **normál boot**. `EN` = 3,3 V → Q55 source ~1,7–1,8 V-ra emeli a lábat (a drain=VDD_EXT korlátoz) → **forced download** a következő PWRKEY-nél. Így a láb sosem lát 1,8 V-nál többet. *(Miért követő és miért nem sima felhúzó/P-MOS: 14.6.)*

### USB_VBUS – konstans 5 V (a Discord-terv)
- `+5V` → **FB41 (v. 0 Ω)** → USB_VBUS (24); **C48** → GND.
- Következmény: a modem folyamatosan „USB bedugva” állapotot lát → **sleep/PSM nem megy**. USB-tápos pult-eszköznél ez rendben van; ha valaha akksis alvás kell, ide egy P-MOS-os kapuzás jönne (új alkatrésztípus – csak akkor, ha tényleg kell). *(Hivatkozás: 14.5.)*

---

## 7. SIM foglalat – `J40` (nanoSIM, csak 1,8 V!)

- **SIM_VDD (18)** → J40 VCC; **C47 100 nF** a foglalatnál
- **SIM_CLK (16)** → **R45 22 Ω** → CLK; **SIM_RST (17)** → **R46 22 Ω** → RST
- **SIM_DATA (15)** → **R47 22 Ω** → I/O — **külső pull-up TILOS/felesleges** (belül 20 kΩ)
- J40 GND → GND; **D43** ESD-tömb (**≤15 pF**) a vonalakra, a foglalatnál

Layout: foglalat **távol az antennától**; SIM_CLK GND-guarddal; rövid, el nem ágazó vonalak.

---

## 8. Antenna – `RF_ANT (32)` → `MODEM_ANT`

Sorrendben a 32-es lábtól:
1. **C50 (C1)** shunt → GND *(DNP)*
2. **R48 (R1) 0 Ω** soros
3. **C51 (C2)** shunt → GND *(DNP)*
4. **C49 (C3) 100 pF** soros (DC-block + ESD)
5. → `MODEM_ANT`; **D42** TVS: `MODEM_ANT` ↔ GND

A C1/C2 értékét az antenna hangolása adja. Megjegyzés: a Discordon chip/PCB-antenna volt a terv, a gyökérben most **U.FL** van – a modem-lapi topológia **mindkettőhöz ugyanez**, csak az illesztőértékek változnak, tehát itt nincs teendő. `MODEM_ANT`: 50 Ω, rövid, RF-szabályok. **GNSS_ANT (68) nyitva.**

---

## 9. NETLIGHT LED – `NETLIGHT (41)` (BSS138-cal)

- **NETLIGHT (41)** → **Q56 gate**; **R50 100 kΩ** gate ↔ GND
- **Q56 source** → GND; **Q56 drain** → **LED40 katód**
- **LED40 anód** → **R51 ~470–510 Ω** → **VBAT-net** (a FB40 utáni oldal)

NETLIGHT magas → LED világít. Villogásminták: 64/800 ms = nincs hálózat; 64/3000 = regisztrálva; 64/300 = adat; ki = off/PSM.

---

## 10. Nettábla (gyors ellenőrzéshez)

| Net | Mi lóg rajta |
|---|---|
| `VDD_EXT` | U40/40, C44, R62, R63, R64, R65, R66, Q55 drain, TP |
| `MODEM.NRST` | Q40 gate (+R41m) |
| `MODEM.TXD` | Q50 gate (+R60→3V3) |
| `MODEM.RXD` | Q52 drain (+R67→3V3) |
| `MODEM.RTS` | Q51 gate (+R61→3V3) |
| `MODEM.CTS` | Q53 drain (+R68→3V3) |
| `MODEM.NINT` | Q54 drain (+R69→3V3) |
| `MODEM_PROG.EN` | Q55 gate |
| `MODEM_PROG.USB_D+ / D-` | U40/25, U40/26 |
| `MODEM_ANT` | C49 után, D42 |
| `+5V` | FB41 → USB_VBUS |
| `+3V3` | FB40 (→VBAT), R60/R61/R67/R68/R69 |

---

## 11. Layout / gyártási emlékeztetők

- **VBAT**: bulk + FB40 + TVS a lábhoz a lehető legközelebb; trace **≥1 mm**; ha van hely, 3× 100 µF.
- Modul köré **≥3 mm keepout**; footprint egyeztetés a Fig 4-gyel.
- **RF** 50 Ω, rövid; **USB D+/D-** 90 Ω diff; **SIM** távol az antennától.
- A FET-fokozatok drain-felhúzói lehetőleg a fogadó oldal közelébe (rövid stubok).
- **Test point**: `VDD_EXT` és `USB_BOOT` (az adatlap kéri), opc. UART2_TXD (boot-log).

---

## 12. Beültetési checklist

- [ ] SIM7080G szimbólum+footprint importálva, footprint ↔ Fig 4 egyeztetve
- [ ] Minden GND láb (a központi pad is) → `GND`
- [ ] VBAT: `+3V3` → FB40 → VBAT, bulk (2–3× 100µF + 1µF + 100nF) + TVS a lábnál
- [ ] VDD_EXT: C44 + test point
- [ ] PWRKEY: Q40 + R40(1k) + R41m(100k) + SW40 + D40
- [ ] Szintillesztő: **5× BSS138** (Q50–Q54) a fenti táblák szerint; gate-felhúzók: Q50/Q51→3V3, Q52–Q54→**VDD_EXT**; drain-felhúzók: Q50/Q51→VDD_EXT, Q52–Q54→3V3
- [ ] ⚠️ Firmware-jegyzet a lapra (text): „UART invertált – uart_set_line_inverse mind a 4 vonalra; NINT: RI = felfutó él”
- [ ] SPI_MOSI (49) **semmilyen pull nélkül**, No-Connect
- [ ] USB: D+/D- bekötve; USB_BOOT: Q55 + R42 + D41 + test point
- [ ] USB_VBUS: `+5V` + FB41 + C48
- [ ] SIM: J40 + 3× 22Ω + C47 + D43; DATA-n nincs külső pull-up
- [ ] Antenna: C50(DNP)/R48(0Ω)/C51(DNP)/C49(100pF) + D42 → `MODEM_ANT`; GNSS_ANT nyitva
- [ ] NETLIGHT: Q56 + R50 + LED40 + R51 VBAT-ról
- [ ] Minden nem használt láb No-Connect flaggel; ERC hibátlan; a 3 hierarchikus címke illeszkedik a gyökérhez

---

## 13. Amit érdemes fejben tartani (kompromisszumok)

- **Invertált UART = firmware-függés.** A hardver csak az `uart_set_line_inverse()`-szel együtt működik. Ez a BSS138-megoldás ára a TXB-hez képest – cserébe nincs plusz alkatrésztípus. Teszteléskor ez legyen az első gyanúsított, ha „néma” a modem.
- **3,3 V-os VBAT:** a modul 2,7–4,8 V-ot vár (tip. 3,8 V). A 0,5 A-es adási tüskék IR-esése miatt a bulk és az LDO terhelhetősége kritikus; ha méréskor adás alatt 3,0 V alá esik, több bulk vagy dedikált buck (Fig 6) kell.
- **`EN` polaritás:** gyökérben ellenőrizve, **aktív magas** (jumper behúzva = programozó mód).
- **RTS/CTS:** funkcionálisan „straight” (ESP RTS → modul RTS; modul CTS → ESP CTS), csak minden vonal invertált szintű. Ha a flow control gyanús, előbb az inverziót ellenőrizd, csak utána cserélgess vonalat.
- **STATUS nincs kivezetve** – készenlét-detektálás időzítéssel (~1,8 s) + AT-válaszból.
- **VBUS konstans 5 V** → a modem nem tud sleepelni. USB-tápos eszköznél oké; akksis változatnál ezt újra elő kell venni.

---

# 14. Részletes indoklás és hivatkozások (blokkoként: mit → miért → hol az adatlapban)

Minden hivatkozás a **SIM7080G Hardware Design V1.05 (2023-06-14)** dokumentumra vonatkozik, az oldalszámok a PDF lábléce szerint („p.NN” = „NN / 82”).

## 14.1 VBAT tápellátás

- **Mit:** `+3V3` → FB → VBAT(34,35); 2–3× 100 µF + 1 µF + 100 nF + TVS a lábnál; ≥1 mm trace.
- **Miért:** a modul üzemi tartománya **2,7–4,8 V**, adáskor **0,5 A tüskékkel** – a bulk feladata, hogy a tüske alatt a láb ne essen 2,7 V alá (a modul már ez alatt lekapcsolhat). A kondenzátorlánc, a ferrit és a TVS a gyári referencia egy-az-egyben.
- **Hol:** §3.1, **Table 7** (VBAT 2,7/3,8/4,8 V; I_peak 0,5 A) – p.21; §3.1.1 + **Fig 5** (100nF+1µF+3×100µF, FB, TVS; „VBAT trace wider than 1 mm”) – p.21–22; ajánlott TVS-lista: **Table 8** – p.22; „ne kapcsold le a VBAT-ot menet közben” megjegyzés – p.22. (Ha a 3,3 V kevésnek bizonyul: buck-referencia §3.1.2 **Fig 6** – p.23.)

## 14.2 VDD_EXT (1,8 V referencia)

- **Mit:** C44 dekap + test point; innen mennek a modem-oldali felhúzók és a Q55 drain.
- **Miért:** ez a modul saját 1,8 V-os LDO-ja (max **50 mA**) – kifejezetten „külső GPIO-felhúzásra vagy szintillesztő táplálására” való, pontosan erre használjuk. A bekapcsolás után **64 ms**-mal áll fel, tehát mire AT-forgalom lehet (~1,8 s), rég stabil.
- **Hol:** §3.11, **Table 17** (1,75–1,85 V, 50 mA) + **Fig 26** (64 ms) – p.41; test point-kérés: Table 6 megjegyzés – p.18, és §3.4.1 kiemelt megjegyzés – p.32.

## 14.3 PWRKEY

- **Mit:** N-MOS lehúzó (Q40) + 1 kΩ soros + nyomógomb + ESD dióda.
- **Miért:** a PWRKEY belül 1,5 V-ra húzott, aktív-alacsony láb – 3,3 V-os GPIO-val közvetlenül hajtani tilos, kapcsolóelem kell (a gyári ábra tranzisztoros, 1 kΩ sorossal; az ESD-diódát az adatlap külön ajánlja). Időzítések: **bekapcsolás Ton = 1…12,6 s**, **kikapcsolás Toff ≥ 1,2 s**, **12,6 s fölött automatikus reset** (ezért nem szabad tartósan földön hagyni), a portok (UART/USB) és a STATUS ~**1,8 s**-nál élnek; két művelet közt **≥2 s** puffer. *(Discord-korrekció: a „12,6 s a bekapcsoláshoz” tévedés – az a reset-küszöb.)*
- **Hol:** §3.2.1 + **Fig 7** (tranzisztoros hajtás, 1 kΩ, ESD-ajánlás) – p.24; **Fig 8 + Table 9** (Ton 1–12,6 s; Ton(status/uart/usb) 1,8 s; VIH>1,0 V, VIL<0,4 V) – p.25; 12,6 s reset: Table 6 PWRKEY sora – p.16; §3.2.2 + **Table 10** (Toff ≥1,2 s; Toff-on 2 s) – p.26–27; STATUS-megjegyzés – p.27.

## 14.4 UART szintillesztés diszkrét BSS138-cal (a Discord-döntés)

- **Mit:** vonalanként 1 FET: gate=bemenet, source=GND, drain=kimenet+felhúzó; 5 vonal (TXD, RXD, RTS, CTS, RI).
- **Miért így (döntés):** a TXB0108 működne (az adatlap maga is ezt mutatja referenciának), de a csapat döntése szerint **nem éri meg új alkatrésztípust** bevezetni, amikor a BSS138 már szerepel a tervben és filléres – „plusz egy féle alkatrész… plusz komplexitás, plusz figyelem a beültetésnél”. Fontos precedens: **az adatlap saját diszkrét alternatívája is pontosan ez a topológia**, csak BJT-vel (közös emitteres, invertáló fokozat, 4,7 k/10 k értékekkel, MMBT3904 ajánlással) – a mi FET-es változatunk ennek a MOSFET-megfelelője, bázisellenállások nélkül.
- **Miért invertál, és miért nem baj:** a közös source-ú kapcsoló természeténél fogva fordít (gate magas → drain lehúzva). Az ESP32-C6 UART perifériája viszont **hardveresen tud vonal-inverziót** (TXD/RXD/RTS/CTS külön-külön), így a fordítás szoftverből tökéletesen kompenzálható: `uart_set_line_inverse(port, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV | UART_SIGNAL_RTS_INV | UART_SIGNAL_CTS_INV)` (ESP-IDF UART driver; az `esp_modem` a szabványos UART driverre épül, a hívás a DTE létrehozása után kiadható ugyanarra a portra). Nyugalmi állapotok stimmelnek: ESP invertált TX idle = fizikai alacsony → FET zár → modul RXD a VDD_EXT-felhúzón = idle magas; modem TX idle = 1,8 V → FET nyit → ESP RXD fizikai alacsony → `RXD_INV`-vel logikai idle magas. A **RI** ugyanígy fordul: az adatlap szerint alap magas, eseménykor **120 ms alacsony** impulzus (és előbb `AT+CFGRI=1`-gyel engedélyezni kell) → az ESP `NINT` lábán ez **felfutó él** lesz.
- **Szintek és abszolút maximumok:** a modul UART-ja 1,8 V-os: **VIH min 1,17 V**, VIL max 0,63 V – a VDD_EXT-re húzott drain (1,8 V) és a FET ~mΩ–Ω nagyságú lehúzása bőven teljesíti. ⚠️ A modul digitális lábain az **abszolút maximum 2,1 V** – ezért a modem-oldali gate-felhúzók (R64–R66) **kizárólag VDD_EXT-re** mehetnek, 3,3 V-ra kötésük a modult károsítaná.
- **Elég gyors-e a BSS138 (koma kérdése):** a kritikus a felfutás a drain-felhúzón: τ = R·C ≈ 10 kΩ × (10–15 pF) ≈ 100–150 ns, a 10–90% felfutás ~2,2τ ≈ 250–330 ns. 115200 baudon a bitidő 8,68 µs → a felfutás a bitidő ~3–4%-a, bőségesen jó; 460800-on ~15% (még oké); 921600-hoz cseréld a drain-felhúzókat 4,7 kΩ-ra. A modem autobaud készlete amúgy is 9600–115200, az `esp_modem` tipikusan 115200-on jár – a 3,686 Mbps-os felső határt ez a fokozat nem célozza (nem is kell). Peremeset: a BSS138 Vgs(th) szórása 0,8–1,5 V, azaz a legrosszabb példány 1,8 V-os gate-hajtással (modem→ESP irány) csak 0,3 V túlvezérlést kap – tipikus daraboknál ez bőven elég a 10 k felhúzó lehúzásához (a klasszikus AN10441/SparkFun BSS138-shifter is rutinszerűen megy 1,8 V-on), de ha egy sarzs gyengélkedne, a tünet a magas VOL az ESP RXD-n – ilyenkor mérd meg a Vth-t, mielőtt mást gyanúsítasz.
- **Gate-felhúzó vs. lehúzó (finomság):** a Discord-recept mindkét oldalra felhúzót mond, így is van bekötve. Az ESP-oldali gate-eknél (Q50/Q51, →3V3) ennek az a mellékhatása, hogy amíg az ESP resetben/Hi-Z-ben van, a FET nyit és a modul RXD-jét alacsonyan (break) tartja – ez ártalmatlan (a modem ilyenkor jellemzően ki van kapcsolva, és a UART break-et amúgy is tűri), de ha valaha zavarna, ezeknél a **gate-lehúzó GND-re** a tiszta alternatíva (Hi-Z ESP → modul RXD idle-magas). Normál üzemben a kettő között nincs különbség.
- **Hol:** UART áttekintés + baud-készlet: §3.3 – p.27–28; full-modem bekötés (RTS/CTS straight): **Fig 10** – p.28; UART szintek: **Table 11** – p.29; TXB-referencia: **Fig 12** – p.29; **diszkrét tranzisztoros referencia: Fig 13** – p.29, és a hozzá tartozó megjegyzés (nagysebességű tranzisztor, MMBT3904) – p.30; RI viselkedés + `AT+CFGRI=1`: §3.3.2, **Fig 14** – p.30; abszolút maximum digitális lábakra (2,1 V): **Table 32** – p.54; 1,8 V-os I/O jellemzők (VIH 1,17 V): **Table 34** – p.55. ESP-oldal: ESP-IDF UART driver, `uart_set_line_inverse()` / `UART_SIGNAL_*_INV` (ESP32-C6 UART fejezet, docs.espressif.com).

## 14.5 USB adat + VBUS

- **Mit:** D+/D- egyenesen a muxhoz (90 Ω diff); VBUS konstans `+5V` ferriten át + dekap.
- **Miért:** az USB a modul firmware-frissítő és debug-csatornája; a D± vonalakon az adatlap <3 pF-es ESD-t kér – ezt a gyökérbeli PI3USB221 mux beépített ESD-je adja, ezért a modem lapon nem duplázzuk. A VBUS a „be van dugva az USB” detektálás: **3,5–5,25 V** közt kell lennie, e nélkül a modul nem áll download módba. A konstans 5 V a Discord-terv („a VBUS meg konstans megkapná az 5V-ot”), az ára ismert: **USB-t érzékelő modul nem megy sleepbe** – az adatlap explicit sleep-feltétele, hogy a VBUS-tápot le kell választani. USB-tápos pultnál irreleváns, akksis változatnál ide P-MOS-os kapuzás kellene (tudatosan elhagyva, mert új alkatrésztípus lenne).
- **Hol:** §3.4 (VBUS 3,5–5,25 V) + **Fig 15** (ESD <3 pF a D±-on) – p.31; 90 Ω diff impedancia + test point megjegyzés – p.32; sleep-feltétel („Connected USB can't enter into sleep mode… disconnect the power supply for USB_VBUS first”): §5.3.2 – p.57; ajánlott üzemi VBUS-tartomány: **Table 33** – p.54.

## 14.6 USB_BOOT kapuzás (Q55 követő)

- **Mit:** BSS138: gate=`EN`, drain=`VDD_EXT`, source→10 kΩ→USB_BOOT; TVS + test point a lábon.
- **Miért:** a láb **normál bootnál nem lehet felhúzva** („DO NOT PULL UP DURING NORMAL POWER UP”), download módhoz viszont **VDD_EXT-re kell húzni, mielőtt** a PWRKEY-t lenyomod – a gyári referencia (Fig 16) pontosan egy VDD_EXT→10 kΩ→láb felhúzás, kézi jumperrel. A Q55 ugyanez, csak a jumpert az `EN` jel váltja ki elektronikusan. Azért „követő” (drain a VDD_EXT-en, source a láb felé), mert a 3,3 V-os `EN` **közvetlenül nem érintheti** az 1,8 V-os lábat (abs. max 2,1 V), a követő kimenete viszont fizikailag nem tud a VDD_EXT (1,8 V) fölé menni – worst-case Vth (1,5 V) mellett is ~1,6–1,8 V-ra áll be, ami fölötte van a láb VIH-jének (1,17 V). `EN` alacsony állapotát a gyökér 10 kΩ-os lehúzója (R41) definiálja, a láb nyugalmi alacsonyát a belső lehúzó (DI,PD) adja. Az időzítés a gyári referenciával azonos: a felhúzó rail maga a VDD_EXT, ami 64 ms-ra áll fel – ugyanígy működik a SIMCom saját kapcsolása is.
- **Hol:** §3.4.2 + **Fig 16** (10 kΩ, TVS, test point, VDD_EXT) – p.32; „cannot be pulled up before normal power up”: Table 6 USB_BOOT sora – p.18 és a p.15 megjegyzés; belső lehúzó (DI,PD): Table 6 – p.18; abs. max 2,1 V: **Table 32** – p.54; VIH 1,17 V: **Table 34** – p.55; VDD_EXT 64 ms: **Fig 26** – p.41.

## 14.7 SIM interfész

- **Mit:** 3× 22 Ω soros, 100 nF a VCC-n a foglalatnál, ≤15 pF ESD-tömb; DATA-n nincs külső felhúzó.
- **Miért:** a modul **csak 1,8 V-os SIM-et** támogat (3 V-osat nem, hot-swapet sem – a foglalatválasztásnál ez már be van árazva); a 22 Ω + TVS az ESD-védelem gyári receptje, a TVS kapacitáskorlátja azért szigorú, mert a **SIM_CLK él-idejének 40 ns alatt kell maradnia** (ehhez §3.5.1 szerint <50 pF, a routing-fejezet szigorúbb ≤15 pF-et mond – ez utóbbit követjük). A DATA belül 20 kΩ-mal fel van húzva, külső felhúzó kifejezetten tiltott. A layout-szabályok (antenna-távolság, CLK-guard, rövid vonalak) a le-nem-eső SIM kulcsa.
- **Hol:** csak 1,8 V: §3.5 – p.32, elektromos: **Table 12** – p.33, „nincs 3 V, nincs hot-swap”: megjegyzés – p.33; referencia-kapcsolás (22 Ω, 100 nF, ESDA6V1W5/SMF15C): §3.5.1 + **Fig 17** – p.33; CLK 40 ns + TVS <50 pF – p.34; routing-szabályok + TVS ≤15 pF: §3.5.2 – p.35; belső 20 kΩ: Table 6 SIM_DATA sora – p.16.

## 14.8 Antenna illesztés

- **Mit:** π-hely (C1/C2 DNP, R1=0 Ω) + 100 pF soros DC-block + kis kapacitású TVS, 50 Ω-os vonal a `MODEM_ANT`-ra.
- **Miért:** ez az adatlap gyári topológiája: az R1=0 Ω / C1-C2 „reserved” kiosztás a későbbi antennahangolás helye (az értékeket az antennagyártó adja), a 100 pF a lábat védő DC-block, a TVS pedig az antennaporton bejövő ESD ellen kell – de csak nagyon kis kapacitású (≤0,3 pF) jöhet szóba, különben rontja az illesztést. A trace-veszteség célértékei és az RF-vezetési szabályok (50 Ω, nincs éles törés, GND-viák, távol a gyors jelektől) szintén adatlapiak. GNSS: a 68-as láb nyitva marad – a chipset korlátja miatt **egyidejű mobil + GNSS vétel amúgy sem lehetséges**, és a felhasználási esethez nem is kell.
- **Hol:** illesztő + magyarázat (R1=0R default, C1/C2 reserved, C3=100 pF): §4.2 + **Fig 28** – p.47–48; ajánlott RF TVS (BLE5V0CR05UB, 0,05 pF): **Table 28** – p.48; trace-veszteség célok: **Table 27** – p.47; RF-vezetési szabályok: §4.4.1 – p.51; GNSS-egyidejűség korlát: §4.3.2 megjegyzés – p.49; (GNSS TVS, ha valaha kellene: **Table 29** – p.50).

## 14.9 NETLIGHT

- **Mit:** Q56 (BSS138) low-side kapcsoló, LED + ~510 Ω a VBAT-ról, gate-lehúzó.
- **Miért:** a gyári ábra NPN-es (4,7 k/47 k bázisosztóval) – FET-tel a bázisellenállások elhagyhatók, és megint nem kell új alkatrésztípus; 1,8 V-os gate-hajtással a BSS138 a ~2–3 mA-es LED-áramot gond nélkül kapcsolja (ha egy gyenge Vth-jú darabbal halvány lenne, csökkentsd R51-et). Az ellenállásérték a LED-től függ (az adatlap is így mondja); a villogásminták táblázatból jönnek – debugnál aranyat ér.
- **Hol:** referencia-kapcsolás: §3.9 + **Fig 25** – p.39; állapot-minták: **Table 15** – p.40; „R a LED karakterisztikájától függ” megjegyzés – p.40.

## 14.10 Nem használt lábak – a két csapda és a többi

- **SPI_MOSI (49): semmilyen felhúzás nem érhet hozzá** – FAST BOOT funkció ül rajta, ha boot előtt magas, **a modul nem indul el**. Hol: Table 6 utáni megjegyzés („Before the normal power up, USB_BOOT and SPI_MOSI cannot be pulled up") – p.15, és §3.8 megjegyzés – p.39.
- **DTR (6) nyitva:** a DTR-alapú alvásvezérlést nem használjuk; alapállapotban (`AT+CSCLK=0`) a modul nem megy DTR-sleepbe, így a láb nyugodtan lóghat. Hol: §3.3.2 DTR-leírás – p.30. (A VBUS-döntés miatt sleep amúgy sincs, 14.5.)
- **STATUS (42):** a busz nem hozza ki; a készenlét ~1,8 s után + AT-válaszból detektálható. Hol: Table 9 (Ton(status) 1,8 s) – p.25; STATUS-megjegyzés – p.27.
- **UART2 (22/23):** boot-log jön ki rajta – test point megfontolható. Hol: Table 6 UART2 sora („Debug UART, the boot log will be output during boot up”) – p.17.
- **GNSS_ANT (68), ANT_CONTROL (43/44), I2C, PCM, ADC, GPIO-k, NC-k:** mind „if unused, keep open” az adatlap szerint – Table 6 – p.15–18.
- Külső ESD általában: a modul saját láb-ESD-je a nem-RF padokon csak ±1 kV (contact) – ezért kell minden kifelé menő vonalra (SIM, PWRKEY, USB_BOOT, antenna) külső védelem. Hol: §5.5, **Table 40** – p.62.

## 14.11 Firmware-teendők összefoglaló (a hardver ezt feltételezi)

1. **UART inverzió** mind a 4 vonalon: `uart_set_line_inverse(port, TXD|RXD|RTS|CTS _INV)` – e nélkül a modem „néma”.
2. **NINT/RI:** megszakítás **felfutó élre**; előtte `AT+CFGRI=1` a modemen (Fig 14, p.30).
3. **PWRKEY szekvencia:** be: NRST magas ≥1 s (soha nem ≥12,6 s!), utána NRST alacsony; várj ~1,8–2 s-ot az első AT előtt; ki: NRST magas ≥1,2 s; két művelet közt ≥2 s (Table 9/10, p.25–27).
4. **Baud:** 115200 fix beállítás javasolt (autobaud-készlet: 9600–115200; §3.3, p.27–28); 10 k drain-felhúzókkal ez a komfortzóna.
5. **Kikapcsolás áramtalanítás előtt:** PWRKEY vagy `AT+CPOWD=1`, csak utána VBAT-bontás (fájlrendszer-védelem; §3.2.2 megjegyzés, p.26).

---

*Végső ellenőrzésnek érdemes végigmenni az adatlap saját checklistjén is: §8.2, Table 48 (schematic) + Table 49 (PCB layout) – p.75–76. A fenti bekötés minden pontját teljesíti, kivéve a tudatos eltéréseket: 3,3 V-os VBAT (14.1) és konstans VBUS (14.5).*
