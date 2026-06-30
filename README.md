# CubeSat Telemetry Link Simulator

A self-contained C++17 simulation of a spacecraft telemetry downlink and the
ground-station receiver that decodes it. A simulated satellite frames
housekeeping telemetry into packets, sends them over a noisy radio channel that
randomly flips bits, and a byte-stream decoder reconstructs the frames with a
state machine, validates them with a CRC, and shows the live values and link
statistics on a terminal dashboard.

No external dependencies — just `g++` and the standard library. Everything runs
on a laptop, far from any hardware.

```
  Satellite ──► Framer ──► Noisy Channel ──► Decoder ──► Dashboard
  (sensors)    (encode)    (bit flips)      (state      (live ANSI
                                             machine)    display)
```

## Why this project

It exercises the core skills of an embedded-software role on a spacecraft
on-board computer, all in software you can run and verify without any
electronics:

| Skill | Where it lives |
|-------|----------------|
| Hardware-near C/C++, communication protocols | byte framing: SYNC + header + payload + CRC (`framer`, `decoder`) |
| ISR-style byte processing (FreeRTOS UART pattern) | `Decoder::feed()` — one byte at a time, **no blocking, no heap** |
| Software test methods / test environment | the fault-injecting `channel` + the `tests/` ladder, incl. a fuzz test |
| ADC → engineering units, RTC timestamps | scaled integer telemetry fields in `protocol.hpp` |
| Systems thinking | the full encode → channel → decode → display pipeline |

The protocol uses the real CCSDS sync marker (`0x1ACFFC1D`) and the standard
CRC-16/CCITT-FALSE, so it is a recognizable, standards-based artifact rather
than an arbitrary exercise.

## Frame format

23 bytes per frame; all multi-byte fields are **big-endian** (network/space byte
order). The CRC covers the 17 bytes of header + payload — not the SYNC word and
not the CRC field itself.

```
Offset  Size  Field          Value / meaning            Covered by CRC?
------  ----  -------------  -------------------------  ---------------
  0      4    SYNC           0x1A 0xCF 0xFC 0x1D        no
  4      1    version_type   0x01                       yes
  5      2    seq_count      increments per frame       yes
  7      1    payload_len    13                         yes
  8     13    payload        serialized Telemetry       yes
 21      2    crc16          CRC-16/CCITT-FALSE         (this is the CRC)
```

Payload (13 bytes): `timestamp_ms` (u32) · `batt_mv` (u16) · `temp_eps_c10`
(i16) · `temp_obc_c10` (i16) · `attitude_cdeg` (i16) · `mode` (u8).

## Build and run

```sh
make            # build the simulator (satlink) and the tests (satlink_tests)
make run        # launch the live dashboard (auto-sweeping link quality)
make test       # run the full test suite
make snapshot   # render one static dashboard frame and exit
```

`./satlink` options:

```
--ber <rate>     fixed bit error rate (e.g. 1e-3); default: automatic sweep
--seed <n>       RNG seed for reproducible noise (default 1)
--rate <n>       telemetry frames per second (default 8)
--duration <s>   stop after s simulated seconds (default: run forever)
--once           render one snapshot frame and exit
--no-color       disable ANSI colours
--no-garbage     do not inject inter-frame line noise
--help           show help
```

With no arguments the link quality cycles from clean to noisy and back, so you
can watch valid frames keep decoding while corrupted ones get caught by the CRC:
the **Frame yield** drops and the **CRC errors** counter climbs as the bit error
rate rises, then recovers when the link clears.

## Source layout

```
include/                       src/
  protocol.hpp     frame format, Telemetry, constants
  serialization.*  big-endian (de)serialization of fields
  crc16.*          CRC-16/CCITT-FALSE
  framer.*         transmitter: Telemetry -> wire frame
  channel.*        noisy channel: independent per-bit flips
  decoder.*        receiver: byte-stream state machine  <-- the core
  satellite.*      simulated telemetry source (sine models)
  dashboard.*      live ANSI terminal display
                   main.cpp   the end-to-end simulation loop
tests/
  test_main.cpp    the test ladder (CRC, serialization, framing, decoder, fuzz)
```

## Tests

`make test` runs twelve groups, bottom-up, each proving one property:

1. CRC matches the canonical check value `0x29B1` for `"123456789"`.
2. Serialization round-trips every field (including negatives) and is big-endian.
3. The framer produces a well-formed frame with an incrementing sequence number.
4. A clean frame decodes back to the original telemetry.
5. Leading garbage is skipped — the decoder hunts for SYNC.
6. Two frames back-to-back both decode; the sequence number advances.
7. A single flipped bit in the payload is caught by the CRC.
8. A flipped bit in the CRC field is caught.
9. After a truncated frame, the decoder re-aligns and the next frame decodes.
10. A malformed header is rejected and the decoder resyncs.
11. Missing sequence numbers are counted as dropped frames.
12. **Torture test:** one million random bytes never crash or wedge the decoder,
    and a real frame still decodes afterward.

## Design notes

- **No dynamic allocation, no blocking.** The decoder owns fixed-size buffers
  and processes exactly one byte per call — the same shape as a UART receive
  interrupt on a microcontroller.
- **Continuous SYNC detection.** A 32-bit rolling register is matched on every
  byte in every state, so a real SYNC word re-aligns the receiver immediately
  and one bad frame cannot cascade into several lost ones.
- **Defensive parsing.** The header's version and length are validated before
  any byte is trusted; an out-of-range length can never drive an over-read.
- **Explicit byte order.** Fields are serialized one at a time in big-endian,
  never by `memcpy`-ing a struct (which would leak padding and host endianness).

## Possible extensions

- Reed–Solomon or convolutional forward error correction so some corrupted
  frames can be *repaired*, not just detected.
- A second protocol framing (COBS / byte-stuffing) to compare against SYNC-word
  framing.
- Replace the simulated channel with a real serial port (`/dev/ttyUSB0`) and run
  the same decoder against actual hardware.
