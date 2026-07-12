# CLAUDE.md – Kegelautomat Steuerplatine

Kontext für Claude-Sessions in diesem Projekt. Kompakt halten.

## Was ist das Projekt
Nachbau der Steuerung eines Wandkegelautomaten aus den 1970ern („Bowling de Luxe /
Mini Sport Kegler", Fa. Dibisch). Eine neue ESP32-C3-Steuerplatine bildet den
originalen Spielablauf nach. Aktuell: **Hardware-/Platinen-Entwurf** (noch kein Code).

## Anzusteuernde Peripherie des Automaten
- 30× Lampen (5 V, low-side gegen GND)
- 12× Einzelkontakte (schalten gegen GND, keine Matrix)
- 3× Spulen / Münz-Weiche (24 V, low-side)
- 8× 7-Segment-Displays (common cathode)

## Hardware-Architektur (Kernentscheidungen — bereits verifiziert)
- **ESP32-C3 Super Mini** als SPI-Master (3,3 V). Herausgeführte GPIOs: 0–10, 20, 21.
- **Gemeinsamer SPI-Bus** (Mode 0) für alle Bausteine; nur der MCP nutzt MISO.
- **Lampen:** 4× 74HC595 (Kaskade, 32 Bit, 30 genutzt) @ 5 V → je 1 Logic-Level-MOSFET.
- **Displays:** MAX7219 (8 Digits common cathode) @ 5 V.
- **Kontakte + Spulen:** MCP23S17 @ **3,3 V** (Port A/B). Kontakte = 12 Eingänge mit
  Pull-up + Interrupt-on-Change → INTA/INTB. Spulen = 3 Ausgänge → IRL540 low-side.
- **74HCT541** = Pegelwandler 3,3 V→5 V, **8/8 Kanäle** belegt: SCK, MOSI, 595-RCLK,
  MAX-LOAD, 595-/OE + 3× Spulen-Gate (IRL540).

### Warum diese Aufteilung (wichtig, nicht wegoptimieren)
- MAX7219 V_IH = 3,5 V und 74HC595@5V V_IH ≈ 3,5 V > 3,3 V → **74HCT541 nötig**.
- MCP23S17 @ 5 V hätte V_IH = 4,0 V → **darum MCP @ 3,3 V** (V_IH = 2,64 V), SPI direkt.
- IRL540-Gate bei 3,3 V grenzwertig → Spulensignale laufen **auch** über den 74HCT541 (5 V).

## ESP32-C3 GPIO-Beschränkungen (beachten!)
- Strapping: **GPIO2, GPIO8, GPIO9** – beim Boot nie auf LOW ziehen.
- **GPIO8** = Onboard-LED (active-low), **GPIO9** = BOOT-Taster → nicht für Peripherie.
- **GPIO2** nur mit Pull-up (HIGH beim Boot) → genutzt als 595-/OE (HIGH = Lampen aus).
- GPIO11–17 = SPI-Flash (n/a), GPIO18/19 = USB, GPIO20/21 = UART0 (frei/Debug).
- Keine Input-only-Pins beim C3.
- Portbelegung im Detail: siehe `Steuerplatine_Doku.md`, Abschnitt 5.

## Dateien
- `Steuerplatine_Doku.md` – **Haupt-Doku**: Funktionsweise, Portbelegung ESP32-C3 +
  Treiber-ICs, Blockdiagramm, Boot-/Sicherheitsverhalten, offene Punkte.
- `Projekt_Kegelautomat.txt` – ursprüngliche Projektbeschreibung.
- `datasheets/` – MCP23S17, MAX7219/7221, ESP32-C3-Pinout, PCB-Skizze, Foto Automat.

## Status & nächste Schritte
- **Jetzt:** User entwirft die Prototyp-Platine anhand der Doku → dabei ergeben sich evtl.
  noch kleine Änderungen an der Portbelegung/Beschaltung.
- **Danach geplant (Step 2):** C-Firmware mit ESP-IDF / VSCode.
- Offene HW-Punkte: 5-V-Strombudget (30 Lampen), Freilaufdioden an Spulen, IRL540-Strom
  real prüfen, „3 Reserve" in der Beschreibung ist real **1 Reserve** am MCP.

## Konventionen
- Doku-Sprache: **Deutsch**.
- Umgebung: Windows, PowerShell primär.
