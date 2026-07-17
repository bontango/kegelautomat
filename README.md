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
**Layout der Prototyp-Platine fertig**; die Portbelegung ist damit festgezurrt – siehe
[`gpiodefs.h`](gpiodefs.h) und Abschnitt 5 der Doku. Die Firmware (C, ESP-IDF: I²S + FATFS/SD,
595-/MCP-/MAX7221-Ansteuerung) folgt als zweiter Schritt.

## Repository-Inhalt
- [`Steuerplatine_Doku.md`](Steuerplatine_Doku.md) – vollständige technische Doku
- [`gpiodefs.h`](gpiodefs.h) – zentrale GPIO-Zuweisungen der Platine (I²S, SD, Bedienung)
- [`datasheets/`](datasheets/) – eigene PCB-Skizze und Foto des Automaten

> Die Hersteller-Datenblätter (Microchip MCP23S17, Maxim MAX7219/7221) und das ESP32-Pinout
> werden in der Doku als Quellen genannt, aus Urheberrechtsgründen aber nicht mitgeliefert –
> bitte direkt beim Hersteller beziehen.
