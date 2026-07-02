# SIM7080G modem lap – huzalozás lépésről lépésre (csomópont-szintű)

Ez a doksi a `modem_bekotes_v2.md` **végrehajtási párja**: itt minden egyes vezeték le van írva. A logika: minden áramkör-blokkhoz kapsz egy kis rajzot, alatta pedig **csomópontlistát** — egy csomópont azoknak a lábaknak a halmaza, amiket **egy vezetékhálóba** kell kötnöd. Ha a csomópontlistát végigpipálod, a blokk kész. Nem kell semmit értelmezni, csak összekötni.

---

## 0. Konvenciók — ezt olvasd el először

**Csomópont = egy net.** A „Csomópont X: {A, B, C}" azt jelenti: A-t, B-t és C-t kösd össze vezetékkel (bárhogyan, akár címkén keresztül — elektromosan egy pont).

**Kétlábú alkatrészek (R, C, FB, SW, TP):** minden ellenállás/kondi pontosan **két** csomópontban szerepel — egy-egy lába megy mindegyikbe. A két láb felcserélhető (kivéve: LED, polarizált kondi, egyirányú TVS — lásd lent).

**BSS138 (minden Q):** SOT-23 tok, lábak: **1 = Gate (G), 2 = Source (S), 3 = Drain (D)**. KiCad szimbólum: `Device:Q_NMOS_GSD` (a lábszámozása pont egyezik), footprint: `Package_TO_SOT_SMD:SOT-23`.

**LED (KiCad `Device:LED`):** **1. láb = katód (K)**, 2. láb = anód (A). A katód a csíkos/jelölt oldal.

**TVS diódák:** ha kétirányú (bidirectional) típust veszel, mindegy melyik láb hova. Ha egyirányú: **katód (csík) a védett jelre, anód GND-re.**

**Címkék:** a bekötést helyi címkékkel (Add Label, `L`) csináld, ne kilométeres vezetékekkel. **Azonos nevű helyi címkék a lapon belül össze vannak kötve** — tehát pl. a `VDD_EXT` címkét nyugodtan leteheted 8 helyre, az egy net lesz.

**Használt címkenevek ezen a lapon:**
`VBAT_M`, `VDD_EXT`, `MODEM.NRST`, `MODEM.TXD`, `MODEM.RXD`, `MODEM.RTS`, `MODEM.CTS`, `MODEM.NINT`, `MODEM_PROG.EN`, `MODEM_PROG.USB_D+`, `MODEM_PROG.USB_D-`, `MODEM_ANT`
(+ a többi csomópontot elég vezetékkel húzni, nem kell nevet adni nekik)

**Javasolt KiCad szimbólumok:** `Device:R`, `Device:C`, `Device:C_Polarized` (a 100 µF-okhoz, ha polimer/tantál; MLCC-nél sima `C`), `Device:LED`, `Device:FerriteBead`, `Device:D_TVS`, `Switch:SW_Push`, `Connector:TestPoint`, `power:+3V3`, `power:+5V`, `power:GND`.

---

## 1. Munkamenet KiCadben (sorrend)

1. **Könyvtár:** töltsd le a SIM7080G szimbólumot+footprintet (SnapEDA/SamacSys, lásd v2 §1), és add hozzá projektszintű libként (*Preferences → Manage Symbol/Footprint Libraries → Project Specific*). Helyezd le: ez lesz **U40**.
2. **Busz lecsatlakoztatás:** a lapon már ott van a három hierarchikus címke. A `MODEM{NRST NINT RXD TXD RTS CTS}` címkétől húzz egy rövid **buszt** (*Add Bus*, `B`), majd a buszról **Unfold from Bus** (jobb klikk a buszon) vagy *Add Bus Entry* + vezeték + címke módszerrel csatlakoztasd le mind a 6 tagot, és címkézd őket: `MODEM.NRST`, `MODEM.NINT`, `MODEM.RXD`, `MODEM.TXD`, `MODEM.RTS`, `MODEM.CTS`. Ugyanígy a `MODEM_PROG{USB_D- USB_D+ EN}`-ből: `MODEM_PROG.USB_D-`, `MODEM_PROG.USB_D+`, `MODEM_PROG.EN`. A `MODEM_ANT` sima jel, arról elég egy vezeték.
   *Innentől ezeket a címkéket bárhol újra letéve ugyanahhoz a nethez kapcsolódsz.*
3. Helyezd le az összes alkatrészt a 2. fejezet (v2) BOM-ja szerint, blokkonként csoportosítva.
4. Kösd be a blokkokat az alábbi csomópontlisták szerint (A → L sorrendben).
5. **NC flagek:** a nem használt lábakra *Place No-Connect* (`Q`) — lista az L blokkban.
6. **ERC.** Ha a `VDD_EXT` netre „no driver” hibát dob (mert a lib-szimbólum a 40-es lábat nem power outputnak vette fel), tegyél a netre egy `power:PWR_FLAG`-et.

**U40 bekötött lábai (gyors referencia):**

| Láb | Név | Blokk |
|---|---|---|
| 1 | UART1_TXD | E |
| 2 | UART1_RXD | E |
| 3 | UART1_RTS | E |
| 4 | UART1_CTS | E |
| 7 | UART1_RI | E |
| 15 | SIM_DATA | I |
| 16 | SIM_CLK | I |
| 17 | SIM_RST | I |
| 18 | SIM_VDD | I |
| 20 | USB_BOOT | G |
| 24 | USB_VBUS | H |
| 25 | USB_DP | F |
| 26 | USB_DM | F |
| 32 | RF_ANT | J |
| 34, 35 | VBAT | A |
| 39 | PWRKEY | D |
| 40 | VDD_EXT | C |
| 41 | NETLIGHT | K |
| GND-k | (23 db) | B |
| minden más | — | L (NC) |

---

## BLOKK A — VBAT táp

```
 +3V3                                                      U40
  │                                                    ┌────────┐
  └──[FB40]──┬──────┬──────┬──────┬──────┬─────────────┤34 VBAT │
             │      │      │      │      │        ┌────┤35 VBAT │
            C40    C41    C42    C43    D44       │    └────────┘
           100µ   100µ    1µ    100n    TVS       │
             │      │      │      │      │      (ugyanaz a net)
            GND    GND    GND    GND    GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| A1 | `+3V3` power szimbólum • FB40 egyik láb | — |
| A2 | FB40 másik láb • **U40/34** • **U40/35** • C40 • C41 • C42 • C43 • D44 (katód) | `VBAT_M` |
| A3 | C40 • C41 • C42 • C43 • D44 (anód) másik lábai | GND (külön-külön GND szimbólum mindre) |

> A `VBAT_M` címke azért kell, mert a K blokk (LED) is erről a netről táplálkozik. D44 javaslat: PESDHC2FD4V5B / ESD5651N (adatlap Table 8, p.22).

---

## BLOKK B — GND lábak

Az U40 alábbi lábjaira tegyél **egy-egy GND szimbólumot** (vagy kösd őket egy közös GND-vezetékre):

**8, 13, 19, 21, 27, 30, 31, 33, 36, 37, 45, 63, 66, 67, 69, 70, 71, 72, 73, 74, 75, 76, 77**

Ha a footprintben a hasoldali nagy pad(ok) külön lábként jelennek meg (EP / 69–77), azok is mind GND.

---

## BLOKK C — VDD_EXT (1,8 V referencia sín)

```
 U40/40 ──┬────┬───────────────  „VDD_EXT" címke
          │    │
         C44  TP1 (test point)
        100n
          │
         GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| C1 | **U40/40** • C44 • TP1 • *(és címkén keresztül: R62, R63, R64, R65, R66 egyik lába + Q55/3 — ezek a saját blokkjukban vannak felsorolva, ott csak leteszed rájuk a `VDD_EXT` címkét)* | **`VDD_EXT`** |
| C2 | C44 másik lába | GND |

---

## BLOKK D — PWRKEY (be/ki + gomb)

```
                                   U40/39 (PWRKEY)
                                      │
                     ┌───────┬────────┤
                     │       │        │
                   SW40     D40     [R40 1k]
                     │      TVS       │
                    GND      │     Q40 D (3)
                            GND
 MODEM.NRST ──┬─────────── Q40 G (1)
              │            Q40 S (2) ─── GND
           [R41m 100k]
              │
             GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| D1 | Q40/1 (G) • R41m egyik láb | `MODEM.NRST` |
| D2 | R41m másik láb | GND |
| D3 | Q40/2 (S) | GND |
| D4 | Q40/3 (D) • R40 egyik láb | — |
| D5 | R40 másik láb • **U40/39** • SW40 egyik láb • D40 (katód) | — |
| D6 | SW40 másik láb | GND |
| D7 | D40 (anód) | GND |

---

## BLOKK E — Szintillesztő (5× BSS138) ⚠️ a lap lelke, itt figyelj

Mind az 5 vonal **ugyanaz a séma**, csak más lábakkal és más felhúzó-sínekkel. A minta:

```
                (felhúzó sín)                       (felhúzó sín)
                     │                                   │
                 [R_gate                             [R_drain
                  100k]                                10k]
                     │                                   │
 BEMENET ────────────┴──── G(1)│         │D(3) ──────────┴──────── KIMENET
                               │  BSS138 │
                               │  S(2)   │
                                   │
                                  GND
```

**E-A vonal: ESP küld → modem fogad** (Q50)

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| E1 | Q50/1 (G) • R60 egyik láb | `MODEM.TXD` |
| E2 | R60 másik láb | `+3V3` |
| E3 | Q50/2 (S) | GND |
| E4 | Q50/3 (D) • R62 egyik láb • **U40/2** (UART1_RXD) | — |
| E5 | R62 másik láb | `VDD_EXT` |

**E-B vonal: ESP RTS → modem RTS** (Q51)

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| E6 | Q51/1 (G) • R61 egyik láb | `MODEM.RTS` |
| E7 | R61 másik láb | `+3V3` |
| E8 | Q51/2 (S) | GND |
| E9 | Q51/3 (D) • R63 egyik láb • **U40/3** (UART1_RTS) | — |
| E10 | R63 másik láb | `VDD_EXT` |

**E-C vonal: modem küld → ESP fogad** (Q52) — ⚠️ itt a gate-felhúzó a **VDD_EXT-re** megy!

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| E11 | **U40/1** (UART1_TXD) • Q52/1 (G) • R64 egyik láb | — |
| E12 | R64 másik láb | **`VDD_EXT`** ⚠️ (nem 3V3!) |
| E13 | Q52/2 (S) | GND |
| E14 | Q52/3 (D) • R67 egyik láb | `MODEM.RXD` |
| E15 | R67 másik láb | `+3V3` |

**E-D vonal: modem CTS → ESP CTS** (Q53)

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| E16 | **U40/4** (UART1_CTS) • Q53/1 (G) • R65 egyik láb | — |
| E17 | R65 másik láb | **`VDD_EXT`** ⚠️ |
| E18 | Q53/2 (S) | GND |
| E19 | Q53/3 (D) • R68 egyik láb | `MODEM.CTS` |
| E20 | R68 másik láb | `+3V3` |

**E-E vonal: modem RI → ESP NINT** (Q54)

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| E21 | **U40/7** (UART1_RI) • Q54/1 (G) • R66 egyik láb | — |
| E22 | R66 másik láb | **`VDD_EXT`** ⚠️ |
| E23 | Q54/2 (S) | GND |
| E24 | Q54/3 (D) • R69 egyik láb | `MODEM.NINT` |
| E25 | R69 másik láb | `+3V3` |

**Ellenőrző szabály, amivel önmagad tudod validálni:** minden FET-nél a **gate-felhúzó ugyanarra a feszültségre megy, amilyen szintű a bemenet** (ESP-oldali bemenet → 3V3; modul-oldali bemenet → VDD_EXT), a **drain-felhúzó pedig arra, amilyen szintű a fogadó fél** (modul bemenete → VDD_EXT; ESP bemenete → 3V3). Ha ez stimmel mind az 5 vonalon, jó.

> ⚠️ Tegyél a blokk mellé egy szöveges jegyzetet a schematicra: **„UART INVERTÁLT — fw: uart_set_line_inverse(TXD|RXD|RTS|CTS); RI/NINT: esemény = felfutó él"**.

---

## BLOKK F — USB adat

```
 MODEM_PROG.USB_D+ ──────── U40/25 (USB_DP)
 MODEM_PROG.USB_D- ──────── U40/26 (USB_DM)
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| F1 | **U40/25** | `MODEM_PROG.USB_D+` |
| F2 | **U40/26** | `MODEM_PROG.USB_D-` |

(Nincs más alkatrész — az ESD a gyökérbeli muxban van.)

---

## BLOKK G — USB_BOOT kapuzás (Q55 követő)

```
                    VDD_EXT
                       │
                    D(3)│
 MODEM_PROG.EN ─── G(1)│ Q55
                    S(2)│
                       └──[R42 10k]──┬── U40/20 (USB_BOOT)
                                     │
                              ┌──────┤
                             D41    TP2
                             TVS  (test point)
                              │
                             GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| G1 | Q55/1 (G) | `MODEM_PROG.EN` |
| G2 | Q55/3 (D) | `VDD_EXT` |
| G3 | Q55/2 (S) • R42 egyik láb | — |
| G4 | R42 másik láb • **U40/20** • D41 (katód) • TP2 | — |
| G5 | D41 (anód) | GND |

(Gate-lehúzó nem kell — az `EN`-t a gyökér 10 kΩ-ja már lehúzza.)

---

## BLOKK H — USB_VBUS (konstans 5 V)

```
 +5V ──[FB41]──┬── U40/24 (USB_VBUS)
               │
              C48 100n
               │
              GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| H1 | `+5V` power szimbólum • FB41 egyik láb | — |
| H2 | FB41 másik láb • **U40/24** • C48 | — |
| H3 | C48 másik láb | GND |

---

## BLOKK I — SIM foglalat (nanoSIM, csak 1,8 V)

A soros 22 Ω-ok a **modul és a foglalat közé** mennek; a 100 nF és az ESD-tömb a **foglalat oldalára** (ahhoz közel a layoutban is).

```
 U40/18 (SIM_VDD) ────────────────┬──────── J40 VCC
                                  ├─ C47 100n ─ GND
                                  └─ D43/a
 U40/16 (SIM_CLK) ──[R45 22Ω]──┬─────────── J40 CLK
                               └─ D43/b
 U40/17 (SIM_RST) ──[R46 22Ω]──┬─────────── J40 RST
                               └─ D43/c
 U40/15 (SIM_DATA) ─[R47 22Ω]──┬─────────── J40 I/O
                               └─ D43/d
 J40 GND ── GND        D43 közös láb ── GND
 J40 VPP ── nyitva (NC flag)   J40 CD/detect (ha van) ── nyitva
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| I1 | **U40/18** • J40 VCC • C47 • D43 „a" vonal-láb | — |
| I2 | C47 másik láb | GND |
| I3 | **U40/16** • R45 egyik láb | — |
| I4 | R45 másik láb • J40 CLK • D43 „b" vonal-láb | — |
| I5 | **U40/17** • R46 egyik láb | — |
| I6 | R46 másik láb • J40 RST • D43 „c" vonal-láb | — |
| I7 | **U40/15** • R47 egyik láb | — |
| I8 | R47 másik láb • J40 I/O • D43 „d" vonal-láb | — |
| I9 | J40 GND (+ árnyékolás/keret, ha külön láb) | GND |
| I10 | D43 közös láb | GND |
| — | J40 VPP, kártyaérzékelő (CD) lábak | **nyitva** (NC flag; hot-swap nincs támogatva) |

> D43 = 4-csatornás ESD-tömb (pl. ESDA6V1W5, SOT-323-5: 1,2,4,5 = vonalak, 3 = közös/GND — **a saját adatlapjából ellenőrizd**, melyik láb a közös!). SIM_DATA-ra **semmilyen külső felhúzót ne** tegyél.

---

## BLOKK J — Antenna illesztő

```
 U40/32 ──┬──[R48 0Ω]──┬──[C49 100pF]──┬──── „MODEM_ANT" (hier. jel felé)
          │            │               │
         C50          C51             D42
         DNP          DNP             TVS
          │            │               │
         GND          GND             GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| J1 | **U40/32** • C50 • R48 egyik láb | — |
| J2 | C50 másik láb | GND |
| J3 | R48 másik láb • C51 • C49 egyik láb | — |
| J4 | C51 másik láb | GND |
| J5 | C49 másik láb • D42 (vonal-láb) | **`MODEM_ANT`** — innen megy a vezeték a lap `MODEM_ANT` hierarchikus címkéjéhez |
| J6 | D42 másik láb | GND |

> C50/C51: tedd le őket, de **DNP** (Do Not Populate) jelöléssel — érték majd az antennahangoláskor. D42: csak ultra-kis kapacitású RF TVS (BLE5V0CR05UB / WE05DGCF-B).

---

## BLOKK K — NETLIGHT LED

```
        VBAT_M
           │
        [R51 510Ω]
           │
        LED40 A (2)
        LED40 K (1)
           │
        D(3)│
 U40/41 ─┬─ G(1)│ Q56
         │  S(2)│── GND
      [R50 100k]
         │
        GND
```

| Csomópont | Ezeket kösd össze | Címke |
|---|---|---|
| K1 | **U40/41** • Q56/1 (G) • R50 egyik láb | — |
| K2 | R50 másik láb | GND |
| K3 | Q56/2 (S) | GND |
| K4 | Q56/3 (D) • LED40/1 (katód) | — |
| K5 | LED40/2 (anód) • R51 egyik láb | — |
| K6 | R51 másik láb | `VBAT_M` |

---

## BLOKK L — Nem használt lábak (NC flag mindre)

Tegyél **No-Connect keresztet** (`Q`) az U40 alábbi lábjaira — összesen **35 db**:

**5, 6** (DCD, DTR) • **9, 10, 11, 12** (PCM) • **14** (GPIO5) • **22, 23** (UART2 — ha akarsz, a 22-re test point mehet a boot-loghoz, akkor arra nem NC) • **28, 29** (NC) • **38** (ADC) • **42** (STATUS) • **43, 44** (ANT_CONTROL) • **46, 47** (NC) • **48, 49, 50, 51** (SPI — ⚠️ a **49**-re véletlenül se kerüljön semmilyen felhúzó!) • **52–56** (NC) • **57–60** (GPIO1–4) • **61, 62** (UART3) • **64, 65** (I2C) • **68** (GNSS_ANT)

---

## Záró ellenőrzés

1. **Számvetés:** 19 bekötött jelláb + 23 GND + 35 NC = 77 láb. Ha az ERC „unconnected pin"-t dob, valamelyik kategóriából kimaradt egy.
2. **FET-szabály** (E blokk végén) stimmel mind az 5 vonalon.
3. `VDD_EXT` címke pontosan ezeken van: U40/40, C44, TP1, R62, R63, R64, R65, R66, Q55/3.
4. `+3V3` címke/szimbólum: FB40, R60, R61, R67, R68, R69.
5. A három hierarchikus címke (MODEM{}, MODEM_PROG{}, MODEM_ANT) mindegyik tagja fel van használva.
6. ERC tiszta (max. PWR_FLAG-gel javított „no driver").
7. A schematicon rajta a szöveges jegyzet az UART-inverzióról.
