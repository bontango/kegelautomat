# CLAUDE.md – Kegelautomat Steuerplatine

Kontext für Claude-Sessions in diesem Projekt. Kompakt halten.

## Was ist das Projekt
Nachbau der Steuerung eines Wandkegelautomaten aus den 1970ern („Bowling de Luxe /
Mini Sport Kegler", Fa. Dibisch). Eine neue **ESP32-S3-WROOM-1-N16R8**-Steuerplatine bildet
den originalen Spielablauf nach. Aktuell: **Hardware-/Platinen-Entwurf**.

## Anzusteuernde Peripherie des Automaten
- 30× Lampen (5 V, low-side gegen GND)
- 16× Kontakt-Eingänge (schalten gegen GND, keine Matrix; 2 Reserve)
- 2× Spulen / Münz-Weiche (24 V, low-side) – **stromlos fallen die Münzen durch** (sicherer Grundzustand)
- 8× 7-Segment-Displays (common cathode) — **fest gemultiplexte 8×8-Matrix** (8 SEG + 8 DIG),
  3 Platinen (2×2 + 1×4) an **einem ~1 m langen 34-pol. Flachband** (Signal–GND verschachtelt)
- **Sound (Zusatz, kein Originalteil):** Audio-Files von SD → MAX98357A (I²S) → Lautsprecher

## Hardware-Architektur (Kernentscheidungen — verifiziert)
- **ESP32-S3-WROOM-1-N16R8** als Master (3,3 V). Nutzbare GPIOs: 0–21, 38–48.
- **Zwei SPI-Busse (Mode 0):**
  - **SPI2** = SD-Kartenleser (MISO 48, MOSI 2, CLK 38, CS 1) — Audio, entkoppelt.
  - **SPI3** = MAX7221 + MCP23S17 gemeinsam (SCLK 17, MOSI 8, MISO 7; CS_MAX 18,
    CS_MCP 15, MCP-INT 6 gespiegelt). Nur der MCP nutzt MISO.
- **I²S:** MAX98357A (LRC 47, BCLK 21, DIN 14). Diese 3 Pins doppeln als DIP1-3, freigegeben via GPIO13.
  DIP-Puffer **muss an 3,3 V** hängen (S3 nicht 5-V-tolerant).
- **Lampen:** 4× 74HC595 (Kaskade, 32 Bit, 30 genutzt) @ 5 V → je 1 Logic-Level-MOSFET.
  An **dedizierten IOs** (SER 3, SRCLK 10, RCLK 9); **nicht** am SPI-Bus.
- **Displays:** **MAX7221** (8 Digits common cathode) @ 5 V, an SPI3. Variante B: Treiber bleibt
  auf der Hauptplatine, 16 Matrixleitungen übers 1-m-Band. **MAX7221 statt 7219** wegen
  slew-limitierter Segmenttreiber (EMV) + echtem CS (MCP-Poll taktet den MAX nicht mit).
  Maßnahmen: Serien-R (68–100 Ω) an CLK/DIN/CS, SPI-Takt ~1 MHz, RSET **12 kΩ** (~40 mA), Abblockung +
  beide GND-Pins, Original-„Widerstände"-Platine entfernen (MAX = Konstantstrom). Details Doku Abschn. 8.
  Digit = **Kingbright SC08-11EWA** (0,8″ cc, V_F≈1,9 V typ → viel Headroom @ 5 V; 30 mA DC / 160 mA Peak).
- **Kontakte:** MCP23S17 @ **3,3 V**, alle 16 IO als Eingänge (14 belegt + 2 Reserve),
  Pull-up + Interrupt-on-Change. `IOCON.MIRROR` = 1 verodert INTA/INTB **intern** → nur
  **INTA** geht an GPIO6, **INTB bleibt offen** (Pins nicht zusammenlöten: ODR-Default =
  Push-Pull). Adresspins **A0/A1/A2 fest auf GND** → Adresse `000` bei HAEN = 0 *und* 1.
- **Spulen:** 2× direkt vom ESP (GPIO 12/11) → 74HCT541 → IRL540 low-side.
- **74HCT541** = Pegelwandler 3,3 V→5 V, **1 IC, 8/8 Kanäle:** SPI3-SCLK, SPI3-MOSI, MAX-CS,
  595-SER/-SRCLK/-RCLK + 2× Spulen-Gate.
- **595-/OE** (GPIO16) läuft **nicht** über den 541, sondern über einen **2N7002** (Open-Drain,
  Pull-up 5 V): Boot-Blanking + Dimmen bleiben, **invertiert → GPIO16 HIGH = Lampen an**.
- Bedienung: ADJUST = GPIO4, SET = GPIO5, DIP-Read-Enable = GPIO13.

### Warum diese Aufteilung (wichtig, nicht wegoptimieren)
- MAX7221 V_IH = 3,5 V (wie 7219) und 74HC595@5V V_IH ≈ 3,5 V > 3,3 V → **74HCT541 nötig**.
- MCP23S17 @ 5 V hätte V_IH = 4,0 V → **darum MCP @ 3,3 V** (V_IH = 2,64 V), SPI direkt.
- IRL540-Gate bei 3,3 V grenzwertig → Spulensignale laufen **auch** über den 74HCT541 (5 V).
- 595 separat (statt am Bus) → 9 Pegelwandler-Leitungen; **/OE via 2N7002** spart den 2. 541.
- S3 ist **nicht 5-V-tolerant** → /OE-Pull-up (5 V) wirkt am 2N7002-Drain, nicht am ESP-Pin.

## ESP32-S3 GPIO-Beschränkungen (beachten!)
- **GPIO33–37 = Octal-PSRAM (N16R8) → nie verwenden.** GPIO26–32 = SPI-Flash (n/a).
- Strapping: **GPIO0, 3, 45, 46** – beim Boot in definiertem Zustand lassen. GPIO0 = BOOT-Taster.
- GPIO3 (Strapping JTAG-Quellwahl) hier als 74HC595-SER, reiner Ausgang – unkritisch bei USB-JTAG.
- **Programmierung/Konsole auf v07 via CH340C an UART0 (GPIO43/44) + USB-C** (Auto-Reset). **GPIO19/20 (native USB) = nicht belegt/`nc`** → frei; native USB optional nachrüstbar.
- GPIO 22–25 existieren nicht; keine Input-only-Pins.
- **4 ESP-Reserve: GPIO 39–42** (JTAG-Pins, als GPIO nutzbar → dann kein JTAG-Debug); zusätzlich IO19/20 frei; 2 Reserve am MCP (GPA6/7).
- Portbelegung im Detail: siehe `Steuerplatine_Doku.md`, Abschnitt 5.

## Dateien
- `Steuerplatine_Doku.md` – **Haupt-Doku**: Funktionsweise, S3-Portbelegung + Treiber-ICs,
  Blockdiagramm, Boot-/Sicherheitsverhalten, offene Punkte.
- `gpiodefs.h` – zentrale GPIO-Zuweisungen (I²S, SD, Bedienung/DIP).
- `Review_v07.md` – Prüfung des v07-Entwurfs (Stand 2026-07-17), Aktionsliste.
- `datasheets/` – MCP23S17, MAX7219/7221, ESP32-S3-Pinout, PCB-Skizze, Foto Automat,
  `Display.jpg` (Original-Display-Verdrahtung: 8×8-Matrix, 34-pol. Stecker),
  `Kegelautomat_Steckerbelegung.xlsx` (**Lampen-/Kontakt-/Digit-Zuordnung**, Blätter
  „Stecker" und „Spielfeld").

## Firmware-Repository (separat)
Der Code liegt **nicht hier**, sondern in
[bontango/kegelautomat-firmware](https://github.com/bontango/kegelautomat-firmware),
Arbeitskopie `C:\Users\bonta\ESP32_source\kegelautomat`
(ESP-IDF 5.5.1, C). Dieses Doku-Repo ist [bontango/kegelautomat](https://github.com/bontango/kegelautomat).
Stand der Firmware: **v0.01**, Programmgerüst mit Selbsttest – Treiber für Lampen (595),
Displays (MAX7221), Kontakte (MCP23S17), Spulen, Audio (WAV von SD), SD-Karte,
Firmware-Update via `update.bin`, Taster/DIP. Der Spielablauf fehlt noch.

**Arbeitsteilung:** Dieses Repo ist die **maßgebliche Quelle für die Elektrik**; das
Firmware-Repo hält nur die abgeleitete Sicht darauf (`main/gpiodefs.h`, `main/hwmap.h`,
`docs/hardware.md`). Wird hier etwas an Portbelegung, Steckerbelegung oder
Platinenrevision geändert, ist es dort nachzuziehen – Details im Abschnitt
„Doku-Vertrag" in `C:\Users\bonta\ESP32_source\kegelautomat\CLAUDE.md`.
Umgekehrt gehören Erkenntnisse aus dem Hardware-Test (z. B. die endgültige Zuordnung
SW10–SW13 auf GPA2–GPA5) **zuerst hierher**.

## Status & nächste Schritte
- **Hardware:** Layout v07 fertig; die offenen Punkte aus `Review_v07.md` (Freilaufdioden,
  24-V-Konzept, 2N7002-Gate-Pulldown, VBUS-Entkopplung) vor dem Fertigen abarbeiten.
- **Firmware:** Gerüst + Selbsttest stehen (siehe oben). Als Nächstes weitere Testmodi
  und dann der Spielablauf.
- Offene HW-Punkte: 5-V-Strombudget (30 Lampen), Freilaufdioden an den 2 Spulen, IRL540-Strom
  real prüfen.

## Konventionen
- Doku-Sprache: **Deutsch**.
- Umgebung: Windows, PowerShell primär.
