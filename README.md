# Kegelautomat – ESP32-C3 Steuerplatine

Neuentwurf der Steuerung für einen Wandkegelautomaten aus den 1970er-Jahren
(„Bowling de Luxe / Mini Sport Kegler", Fa. Dibisch). Eine ESP32-C3-basierte
Platine bildet den originalen Spielablauf nach.

## Der Automat steuert / erfasst
- **30 Lampen** (5 V, low-side) – symbolisieren die Kegel u. a.
- **12 Kontakte** (Einzelkontakte gegen GND)
- **3 Spulen** (24 V) – Münz-Weiche
- **8× 7-Segment-Displays** (common cathode) – Punkteanzeige

## Hardware-Konzept
| Baustein | Aufgabe | Anbindung |
|----------|---------|-----------|
| **ESP32-C3 Super Mini** | zentrale Steuerung (SPI-Master, 3,3 V) | — |
| **4× 74HC595** | 30 Lampen über MOSFETs (Kaskade, 32 Bit) | SPI, 5 V |
| **MAX7219** | 8 Displays (common cathode) | SPI, 5 V |
| **MCP23S17** | 12 Kontakte (Interrupt) + 3 Spulen (IRL540) | SPI, 3,3 V |
| **74HCT541** | Pegelwandler 3,3 V → 5 V (8 Kanäle) | — |

Alle Bausteine hängen an einem gemeinsamen SPI-Bus. Ausführliche Beschreibung,
Portbelegung und Blockdiagramm: **[`Steuerplatine_Doku.md`](Steuerplatine_Doku.md)**.

## Status
Hardware-/Platinen-Entwurf. Firmware (C, ESP-IDF) folgt als zweiter Schritt.

## Repository-Inhalt
- [`Steuerplatine_Doku.md`](Steuerplatine_Doku.md) – vollständige technische Doku
- [`Projekt_Kegelautomat.txt`](Projekt_Kegelautomat.txt) – ursprüngliche Projektbeschreibung
- [`datasheets/`](datasheets/) – eigene PCB-Skizze und Foto des Automaten

> Die Hersteller-Datenblätter (Microchip MCP23S17, Maxim MAX7219) und das ESP32-C3-Pinout
> werden in der Doku als Quellen genannt, aus Urheberrechtsgründen aber nicht mitgeliefert –
> bitte direkt beim Hersteller beziehen.
