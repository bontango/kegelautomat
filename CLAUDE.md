# CLAUDE.md – Kegelautomat Steuerplatine

Kontext für Claude-Sessions in diesem Projekt. Kompakt halten.

## Was ist das Projekt
Nachbau der Steuerung eines Wandkegelautomaten aus den 1970ern („Bowling de Luxe /
Mini Sport Kegler", Fa. Dibisch). Eine neue **ESP32-S3-WROOM-1-N16R8**-Steuerplatine bildet
den originalen Spielablauf nach. Aktuell: **Hardware-/Platinen-Entwurf**.

> **Wechsel C3 → S3 (2026-07-14):** ursprünglich ESP32-C3 mit LEDC-PWM-Sound; wegen der
> Audioqualität auf den S3 umgestellt, um **MAX98357A (I²S)** + **SD-Karte** für echte
> Audio-Files zu nutzen. Größerer Pin-Vorrat → sauberere Bus-Aufteilung.

## Anzusteuernde Peripherie des Automaten
- 30× Lampen (5 V, low-side gegen GND)
- 16× Kontakt-Eingänge (schalten gegen GND, keine Matrix; 2 Reserve)
- 2× Spulen / Münz-Weiche (24 V, low-side)
- 8× 7-Segment-Displays (common cathode)
- **Sound (Zusatz, kein Originalteil):** Audio-Files von SD → MAX98357A (I²S) → Lautsprecher

## Hardware-Architektur (Kernentscheidungen — verifiziert)
- **ESP32-S3-WROOM-1-N16R8** als Master (3,3 V). Nutzbare GPIOs: 0–21, 38–48.
- **Zwei SPI-Busse (Mode 0):**
  - **SPI2** = SD-Kartenleser (MISO 48, MOSI 2, CLK 38, CS 1) — Audio, entkoppelt.
  - **SPI3** = MAX7219 + MCP23S17 gemeinsam (SCLK 11, MOSI 13, MISO 14; CS_MAX 15,
    CS_MCP 16, MCP-INT 17 gespiegelt). Nur der MCP nutzt MISO.
- **I²S:** MAX98357A (LRC 3, BCLK 9, DIN 10). Diese 3 Pins doppeln als DIP1-3, freigegeben via GPIO12.
- **Lampen:** 4× 74HC595 (Kaskade, 32 Bit, 30 genutzt) @ 5 V → je 1 Logic-Level-MOSFET.
  An **dedizierten IOs** (SER 6, SRCLK 7, RCLK 8); **nicht** am SPI-Bus.
- **Displays:** MAX7219 (8 Digits common cathode) @ 5 V, an SPI3.
- **Kontakte:** MCP23S17 @ **3,3 V**, alle 16 IO als Eingänge (14 belegt + 2 Reserve),
  Pull-up + Interrupt-on-Change, INTA/INTB **gespiegelt** → 1 INT-Leitung.
- **Spulen:** 2× direkt vom ESP (GPIO 21/47) → 74HCT541 → IRL540 low-side.
- **74HCT541** = Pegelwandler 3,3 V→5 V, **1 IC, 8/8 Kanäle:** SPI3-SCLK, SPI3-MOSI, MAX-LOAD,
  595-SER/-SRCLK/-RCLK + 2× Spulen-Gate.
- **595-/OE** (GPIO18) läuft **nicht** über den 541, sondern über einen **2N7002** (Open-Drain,
  Pull-up 5 V): Boot-Blanking + Dimmen bleiben, **invertiert → GPIO18 HIGH = Lampen an**.
- Bedienung: ADJUST = GPIO4, SET = GPIO5, DIP-Read-Enable = GPIO12.

### Warum diese Aufteilung (wichtig, nicht wegoptimieren)
- MAX7219 V_IH = 3,5 V und 74HC595@5V V_IH ≈ 3,5 V > 3,3 V → **74HCT541 nötig**.
- MCP23S17 @ 5 V hätte V_IH = 4,0 V → **darum MCP @ 3,3 V** (V_IH = 2,64 V), SPI direkt.
- IRL540-Gate bei 3,3 V grenzwertig → Spulensignale laufen **auch** über den 74HCT541 (5 V).
- 595 separat (statt am Bus) → 9 Pegelwandler-Leitungen; **/OE via 2N7002** spart den 2. 541.
- S3 ist **nicht 5-V-tolerant** → /OE-Pull-up (5 V) wirkt am 2N7002-Drain, nicht am ESP-Pin.

## ESP32-S3 GPIO-Beschränkungen (beachten!)
- **GPIO33–37 = Octal-PSRAM (N16R8) → nie verwenden.** GPIO26–32 = SPI-Flash (n/a).
- Strapping: **GPIO0, 3, 45, 46** – beim Boot in definiertem Zustand lassen. GPIO0 = BOOT-Taster.
- GPIO3 (Strapping JTAG-Quellwahl) hier als I²S-LRC – unkritisch bei USB-JTAG.
- **GPIO19/20 = USB** (Konsole/Flash via USB-Serial/JTAG), **GPIO43/44 = UART0** (Debug) → frei halten.
- GPIO 22–25 existieren nicht; keine Input-only-Pins.
- **0 ESP-Reserve** (alle 12 zuvor freien Pins belegt); 2 Reserve am MCP.
- Portbelegung im Detail: siehe `Steuerplatine_Doku.md`, Abschnitt 5.

## Dateien
- `Steuerplatine_Doku.md` – **Haupt-Doku**: Funktionsweise, S3-Portbelegung + Treiber-ICs,
  Blockdiagramm, Boot-/Sicherheitsverhalten, offene Punkte.
- `gpiodefs.h` – zentrale GPIO-Zuweisungen (I²S, SD, Bedienung/DIP).
- `Projekt_Kegelautomat.txt` – ursprüngliche Projektbeschreibung.
- `datasheets/` – MCP23S17, MAX7219/7221, ESP32-S3-Pinout, PCB-Skizze, Foto Automat.
- `firmware/` – **noch** das alte flashbare LEDC-Sound-Testprojekt (Target esp32c3) aus dem
  C3-Entwurf; wird für die S3-Audiokette (MAX98357A/SD) neu aufgesetzt.

## Status & nächste Schritte
- **Jetzt:** Prototyp-Platine auf S3 entwerfen anhand der Doku.
- **Step 2:** S3-Firmware mit ESP-IDF/VSCode (I²S + FATFS/SD, 595-/MCP-/MAX7219-Ansteuerung).
- Offene HW-Punkte: 5-V-Strombudget (30 Lampen), Freilaufdioden an den 2 Spulen, IRL540-Strom
  real prüfen, DIP-Puffer im I²S-Betrieb hochohmig halten.

## Konventionen
- Doku-Sprache: **Deutsch**.
- Umgebung: Windows, PowerShell primär.
