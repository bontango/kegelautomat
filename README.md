# Kegelautomat – ESP32-S3 Steuerplatine

Neuentwurf der Steuerung für einen Wandkegelautomaten aus den 1970er-Jahren
(„Bowling de Luxe / Mini Sport Kegler", Fa. Dibisch). Eine Platine mit
**ESP32-S3-WROOM-1-N16R8** bildet den originalen Spielablauf nach.

## Der Automat steuert / erfasst
- **30 Lampen** (5 V, low-side) – symbolisieren die Kegel u. a.
- **Kontakte** (Einzelkontakte gegen GND) – erfasst über MCP23S17, **16 Eingänge (2 Reserve)**
- **2 Spulen** (24 V) – Münz-Weiche; **stromlos fallen die Münzen durch** (sicherer Grundzustand)
- **8× 7-Segment-Displays** (common cathode, gemultiplexte 8×8-Matrix, 34-pol. Ribbon ~1 m) – Punkteanzeige
- **Sound** (Zusatz, kein Originalteil): Audio-Files von SD-Karte über **MAX98357A (I²S)**

## Hardware-Konzept
| Baustein | Aufgabe | Anbindung |
|----------|---------|-----------|
| **ESP32-S3-WROOM-1-N16R8** | zentrale Steuerung (3,3 V) | — |
| **4× 74HC595** | 30 Lampen über MOSFETs (Kaskade, 32 Bit, 2 Reserve) | dedizierte IOs, 5 V |
| **MAX7221** | 8 Displays (common cathode, gemultiplexte 8×8-Matrix) | SPI3, 5 V |
| **MCP23S17** | 16 Kontakte (Interrupt, 2 Reserve) | SPI3, 3,3 V |
| **2× IRL540** | 2 Spulen (24 V, low-side) | ESP-IO → 74HCT541 → Gate |
| **74HCT541** | Pegelwandler 3,3 V → 5 V (1 IC, 8 Kanäle) | — |
| **2N7002** | 595-`/OE` (Open-Drain, Boot-Blanking + Dimmen) | ESP-IO, Pull-up 5 V |
| **MAX98357A** | I²S-Class-D-Verstärker → Lautsprecher | I²S |
| **SD-Kartenleser** | Audio-Files (WAV) | SPI2 |

Zwei getrennte SPI-Busse (**SPI2 = SD-Karte**, **SPI3 = Display + Kontakte**) plus ein
**I²S**-Zweig für den Ton. Ausführliche Beschreibung, Portbelegung und Blockdiagramm:
**[`Steuerplatine_Doku.md`](Steuerplatine_Doku.md)**.

## Status
**Schaltplan und Layout in Revision v1.0 fertig.** Alle Punkte aus dem Review des
v07-Prototyps sind abgearbeitet – Freilaufdioden und 24-V-Versorgung (extern),
2N7002-Gate-Pulldown, USB-VBUS-Entkopplung, Pull-ups an den SPI3-Chip-Selects – dazu die
korrigierte Belegung des Displaysteckers. Verifikation: [`Review_v10.md`](Review_v10.md).

Die Portbelegung ist damit festgezurrt – siehe [`gpiodefs.h`](gpiodefs.h) und Abschnitt 5
der Doku.

Die **Firmware** (C, ESP-IDF 5.5.1) liegt im eigenen Repository
[bontango/kegelautomat-firmware](https://github.com/bontango/kegelautomat-firmware)
(Arbeitskopie: `C:\Users\bonta\ESP32_source\kegelautomat`).
Stand dort: **v0.01** – Programmgerüst mit Selbsttest –
Treiber für Lampen, Displays, Kontakte, Spulen, Audio (WAV von SD) und
Firmware-Update von SD stehen, der Spielablauf folgt.

Dieses Repository bleibt die maßgebliche Quelle für die Elektrik; Änderungen an
Port-/Steckerbelegung sind im Firmware-Repo nachzuziehen (dort `main/gpiodefs.h`,
`main/hwmap.h`, `docs/hardware.md`).

## Repository-Inhalt
- [`Steuerplatine_Doku.md`](Steuerplatine_Doku.md) – vollständige technische Doku
- [`gpiodefs.h`](gpiodefs.h) – zentrale GPIO-Zuweisungen der Platine (I²S, SD, Bedienung)
- [`Review_v10.md`](Review_v10.md) – Verifikation der aktuellen Revision v1.0
- [`Review_v07.md`](Review_v07.md) – Review des Prototyps v07 (historisch)
- [`datasheets/`](datasheets/) – Schaltplan/Layout als PDF, eigene PCB-Skizze, Foto des Automaten

> Die Hersteller-Datenblätter (Microchip MCP23S17, Maxim MAX7219/7221) und das ESP32-Pinout
> werden in der Doku als Quellen genannt, aus Urheberrechtsgründen aber nicht mitgeliefert –
> bitte direkt beim Hersteller beziehen.
