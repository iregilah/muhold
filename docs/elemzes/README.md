# Soronkénti kódelemzés — olvasási sorrend

Ez a mappa a **teljes kód soronkénti elemzése**, magyarul, minden lényeges
nyelvi döntéssel. Olyasvalakinek készült, aki a **C++11 előtti** nyelvet ismeri
(plusz osztályok, pointerek, szálak), és a modern elemeket szeretné pótolni.

A legjobb módszer: nyisd meg egymás mellett az **elemzést** és a **forrást**,
amiről szól.

## Sorrend

0. [00_nyelvi_alapozo.md](00_nyelvi_alapozo.md) — **kezdd itt.** Szótár a modern
   (C++11+) nyelvi elemekről: `constexpr`, `enum class`, lambdák, `auto`,
   `{}`-inicializálás, `<random>`, `<chrono>`, bitműveletek, makrók, és amit
   tudatosan *nem* használtam. A többi fájl erre hivatkozik (pl. „00/12" =
   lambdák).
1. [01_protokoll_es_szerializacio.md](01_protokoll_es_szerializacio.md) —
   `protocol.hpp`, `serialization.{hpp,cpp}`, `crc16.{hpp,cpp}`. A vezetékre
   kerülő adat és a big-endian / CRC alapok.
2. [02_ado_oldal.md](02_ado_oldal.md) — `framer.{hpp,cpp}`, `channel.{hpp,cpp}`.
   A keret összerakása és a zajos csatorna.
3. [03_dekoder.md](03_dekoder.md) — `decoder.{hpp,cpp}`. **A projekt magja:** a
   bájtfolyam-állapotgép, a görgő szinkronregiszter, a védekező feldolgozás.
4. [04_szimulator_es_kijelzo.md](04_szimulator_es_kijelzo.md) —
   `satellite.{hpp,cpp}`, `dashboard.{hpp,cpp}`. A telemetria-forrás és a
   látványos kijelző (sok lambda + a szélesség-igazítás trükkje).
5. [05_fociklus_es_tesztek.md](05_fociklus_es_tesztek.md) — `main.cpp`,
   `tests/test_main.cpp`, `Makefile`. Ahol minden összeáll, és ami bizonyítja.

## Kapcsolódó

- [../HOGYAN_MUKODIK.md](../HOGYAN_MUKODIK.md) — magasabb szintű, fogalmi
  áttekintés (ha előbb a „miért"-et akarod, mint a soronkénti „hogyan"-t).
- [../../README.md](../../README.md) — a projekt angol, portfólió-arcú leírása.
