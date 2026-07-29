# Steuerplatine Kegelautomat – Funktionsweise & Portbelegung

**Projekt:** Nachbau der Steuerung eines Wandkegelautomaten „Bowling de Luxe / Mini Sport Kegler" (Fa. Dibisch, ~1970er)
**Zentrale Steuerung:** ESP32-S3-WROOM-1-N16R8
**Platinenrevision:** **v1.0** (`datasheets/Kegelautomat_v10_SCH.PDF` / `_PCB.PDF`)
**Stand:** 2026-07-29

---

## 1. Übersicht & Zielsetzung

Der Automat ist ein Wandgerät, bei dem mit einem Hebel Kugeln „eingeschossen" werden, um möglichst viele Kegel umzuwerfen. Die **Kegel werden durch Lampen symbolisiert**, die erzielten **Punkte auf den 7-Segment-Displays** angezeigt. Die Steuerplatine bildet den originalen Spielablauf nach. Anzusteuern / auszuwerten sind:

| Menge | Element | Elektrische Eigenschaft |
|------:|---------|-------------------------|
| 30×   | Lampen (Kegel-Symbole u. a.) | 5 V, geschaltet gegen **GND** (Low-Side) |
| 16×   | Kontakt-Eingänge (Einzelkontakte, keine Matrix) | schalten gegen **GND**; **2 als Reserve** |
| 2×    | Spulen (Münz-Weiche) | 24 V, geschaltet gegen **GND** (Low-Side) |
| 8×    | 7-Segment-Displays (Punkte) | **common cathode** |
| +     | **Sound** (Zusatz, kein Originalteil) | Audio-Files von SD → **MAX98357A (I²S)** → Lautsprecher |

**Ergebnis der Verifikation:** Alle Komponenten sind mit dem ESP32-S3 steuerbar. Es gibt **zwei getrennte SPI-Busse** – **SPI2** für den SD-Kartenleser (Audio), **SPI3** für MAX7221 (Displays) + MCP23S17 (Kontakte) – dazu einen **I²S**-Zweig für den Ton und **dedizierte IOs** für die Lampen-Kaskade. Ein **74HCT541** pegelt die 3,3-V-Ausgänge auf 5 V für die 5-V-Bausteine. Details und die elektrischen Fallstricke (mit Lösung) siehe Abschnitt 3.

---

## 2. Systemarchitektur

Der ESP32-S3 ist Master aller Busse:

- **SPI2** (Host): SD-Kartenleser – hoher Durchsatz für Audio-Files, entkoppelt vom Rest.
- **SPI3** (Host): MAX7221 + MCP23S17 an einem gemeinsamen Bus (SCLK/MOSI geteilt), je eigene CS-Leitung; nur der MCP nutzt **MISO**.
- **I²S**: MAX98357A (LRC, BCLK, DIN).
- **Dedizierte IOs**: 74HC595-Kaskade (SER, SRCLK, RCLK) – vom Display-Bus entkoppelt; `/OE` über einen 2N7002.
- **2 direkte GPIOs**: Spulen (→ 74HCT541 → IRL540).

```mermaid
graph LR
    ESP["ESP32-S3-WROOM-1-N16R8<br/>3,3 V"]

    subgraph SPI2["SPI2"]
        SD["SD-Kartenleser<br/>Audio-Files (WAV)"]
    end

    subgraph I2S["I²S"]
        AMP["MAX98357A<br/>Class-D-Amp"]
    end

    subgraph LV["3,3-V-Domäne (SPI3)"]
        MCP["MCP23S17<br/>16 Kontakt-Eingänge"]
    end

    subgraph BUF["Pegelwandler"]
        HCT["74HCT541<br/>3,3 → 5 V · 8 Kanäle"]
        Q["2N7002<br/>595-/OE (Open-Drain)"]
    end

    subgraph HV["5-V-Domäne"]
        SR["4× 74HC595<br/>Kaskade, 32 Bit"]
        MAX["MAX7221<br/>Display-Treiber"]
    end

    ESP -- "SPI2: SCLK,MOSI,MISO,CS" --> SD
    ESP -- "I²S: LRC,BCLK,DIN" --> AMP
    AMP --> SPK["Lautsprecher"]
    ESP -- "SPI3: SCLK,MOSI,MISO,CS_MCP,INT" --> MCP
    ESP -- "SPI3-SCLK,MOSI + LOAD (3,3 V)" --> HCT
    ESP -- "595: SER,SRCLK,RCLK (3,3 V)" --> HCT
    ESP -- "GPIO16 (3,3 V)" --> Q
    ESP -- "2× Spule (3,3 V)" --> HCT

    HCT -- "CLK,DIN,CS (5 V, +Serien-R)" --> MAX
    HCT -- "SER,SRCLK,RCLK (5 V)" --> SR
    Q -- "/OE (5 V)" --> SR
    HCT -- "2× Gate (5 V)" --> IRL["2× IRL540<br/>Spulen 24 V"]

    SR --> LAMPS["32× N-MOSFET<br/>→ 30 Lampen 5 V"]
    MAX -- "8 SEG + 8 DIG (34-pol. Ribbon ~1 m)" --> DISP["8× 7-Segment<br/>common cathode<br/>gemultiplexte 8×8-Matrix"]
    MCP --> CONT["16 Kontakte gegen GND<br/>(2 Reserve)"]
```

**Warum SPI3 gemeinsam funktioniert:** Nur der Baustein, dessen CS aktiviert wird, übernimmt die Daten – und der MAX7221 schiebt (anders als der MAX7219) nur bei aktivem CS. SCK/MOSI treiben parallel den 74HCT541-Eingang (→ MAX7221, 5 V) **und** direkt den MCP23S17 (3,3 V) – ein 3,3-V-Ausgang auf zwei 3,3-V-Lasten, unkritisch. Die Lampen-Kaskade hängt an eigenen IOs und wird davon gar nicht berührt.

---

## 3. Spannungsdomänen & Pegelkonzept

Die Platine hat **drei Spannungsdomänen**:

| Domäne | Versorgt | Anmerkung |
|--------|----------|-----------|
| 3,3 V  | ESP32-S3, MCP23S17, MAX98357A-Logik | Logik-Master |
| 5 V    | 74HCT541, 4×74HC595, MAX7221, Lampen, MAX98357A-Endstufe | Treiber, Anzeige, Ton |
| 24 V   | Spulen (über IRL540) | nur Leistungspfad |

### 3.1 Das Kernproblem: 3,3-V-Logik an 5-V-Bausteinen

Der ESP32-S3 gibt an seinen GPIOs nur **3,3 V** aus. Mehrere 5-V-Bausteine verlangen aber eine höhere High-Schwelle (V_IH). Aus den Datasheets:

| Baustein | V_IH (min, garantiert) | Bei 3,3 V vom ESP? | Quelle |
|----------|------------------------|--------------------|--------|
| **MAX7221** (V+ = 5 V) | **3,5 V** | ✗ unter Schwelle (out of spec) | `max7219-max7221.pdf`, Electrical Characteristics (7219/7221 identisch) |
| **74HC595** (VCC = 5 V) | ≈ **3,5 V** (0,7·VCC) | ✗ grenzwertig | 74HC595 Standard-Datasheet |
| **MCP23S17** (VDD = 5 V) | **0,8·VDD = 4,0 V** | ✗ unsicher | `MCP23S17_MIC.pdf`, DC Characteristics (D041) |
| **MCP23S17** (VDD = 3,3 V) | **0,8·VDD = 2,64 V** | ✓ sicher | ″ |

### 3.2 Lösungskonzept

1. **MCP23S17 mit 3,3 V betreiben.** Dann liegt V_IH bei 2,64 V → die 3,3-V-SPI-Signale des ESP werden sicher erkannt. Die SPI-Leitungen zum MCP gehen **direkt** (ungepuffert), MISO/INT kommen mit 3,3 V zurück (ESP-konform). Betriebsspannungsbereich des MCP23S17: 1,8–5,5 V (Datasheet), 3,3 V ist zulässig.

2. **74HCT541 als Pegelwandler 3,3 V → 5 V** für alle Leitungen, die in die 5-V-Bausteine gehen. Der HCT-Eingang erkennt 3,3 V sicher als High (V_IH,HCT = 2,0 V) und treibt am Ausgang volle 5 V. Das löst **MAX7221** und **74HC595** in einem Rutsch.

3. **595 mit 5 V betreiben** – dadurch liefern die 595-Ausgänge 5 V an die Lampen-MOSFET-Gates (sauberes Durchschalten der Logic-Level-MOSFETs).

4. **IRL540-Gate-Ansteuerung ebenfalls über den 74HCT541.** Die 2 Spulen-Steuersignale kommen als 3,3-V-Ausgang direkt aus dem ESP32-S3 (GPIO12/11) und werden im 74HCT541 auf 5 V gepegelt → volle Gate-Spannung an den IRL540. Das behebt die grenzwertige 3,3-V-Gate-Ansteuerung.

5. **595-`/OE` über einen 2N7002 (Open-Drain)** statt über den 74HCT541 – so bleibt der Pegelwandler bei **einem** IC. Details in Abschnitt 6.1.

> **Ergebnis:** Der eine 74HCT541 ist mit **8 von 8 Kanälen** belegt: SPI3-SCLK, SPI3-MOSI, MAX-CS, 595-SER/-SRCLK/-RCLK + 2 Spulen-Gate-Signale. `/OE` läuft separat über den 2N7002. Siehe Abschnitt 6.

### 3.3 5-V-Einspeisung: USB-VBUS ↔ externes Netzteil (Entkopplung)

> **Umgesetzt und am Schaltplan v1.0 verifiziert** (Review-Punkt 4, vorher fehlend).

Das 5-V-Rail wird aus **zwei** Quellen gespeist: dem **externen 5-V-Netzteil an J1** (Volllast inkl. Lampen) und **USB-VBUS** (nur beim Programmieren/Debuggen über USB-C). Damit bei gleichzeitig gestecktem USB **und** externem Netzteil nicht das eine ins andere zurückspeist (Rückstrom in den PC-Port über die PTC-Sicherung F1), ist der VBUS-Zweig **entkoppelt**:

```
USB-VBUS ──► F1 (PTC, MF-MSMF150-2: I_hold 1,5 A) ──► D4 (SS24) ──►┐
                                                                   ├── +5-V-Rail
externes 5-V-Netzteil ─────────── J1 ─────────────────────────────►┘
```

- **Schottky = D4 (SS24)** (2 A / 40 V, SMA). Sperrt Rückspeisung ins USB-Port; niedriger Vf (~0,3–0,4 V bei < 1 A), da der USB-Zweig nur Logik + Display + CH340 trägt (die 30 Lampen bis 3 A laufen ausschließlich über J1, hinter der Diode).
- **Orientierung:** Anode an F1-Seite (VBUS), **Kathode (Ring) am +5-V-Rail** – im Schaltplan v1.0 so gezeichnet.
- **CH340C-VCC** hängt auf der **+5-V-Rail-Seite** (nach der Diode) → sowohl bei USB- als auch bei externer Versorgung sauber versorgt.
- **Hinweis:** Im reinen USB-Betrieb (ohne J1-Netzteil) frisst der Vf ~0,3 V vom Rail. Für ESP-S3-LDO und MAX98357A unkritisch; falls die Platine mal *ausschließlich* über USB laufen soll (nicht nur flashen), Rail unter Last gegenmessen.

---

## 4. ESP32-S3: GPIO-Eigenschaften & Boot-Beschränkungen

Das Modul **ESP32-S3-WROOM-1-N16R8** (16 MB Flash, **8 MB Octal-PSRAM**) führt GPIO **0–21** und **38–48** heraus (GPIO 22–25 existieren nicht). Gesperrt bzw. mit Vorsicht:

| GPIO | Besonderheit | Konsequenz |
|------|--------------|------------|
| **33–37** | **Octal-PSRAM** (wegen „R8") intern belegt | **nie verwenden** |
| **26–32** | SPI-Flash | nicht verwendbar |
| **19, 20** | **USB D-/D+** (nativ, USB-Serial/JTAG) | **nicht belegt** (`nc`) → frei; native USB optional nachrüstbar |
| **43, 44** | **UART0** TX/RX | an **CH340C** (X3, USB-UART-Brücke, Programmierung/Konsole via USB-C) |
| **0** | Strapping (Boot) + BOOT-Taster | reserviert |
| **3** | Strapping (JTAG-Quellwahl) | unkritisch bei USB-JTAG; hier 74HC595-`SER` (reiner Ausgang) – **10-kΩ-Pulldown ab v1.0 bestückt** |
| **45** | Strapping (VDD_SPI) | nicht für Peripherie |
| **46** | Strapping (Boot/ROM-Messages) | nicht für Peripherie |

**Wichtig:**
- Strapping-Pins (0, 3, 45, 46) beim Boot in ihrem definierten Zustand belassen.
- Der ESP32-S3 hat **keine Input-only-Pins** – alle nutzbaren GPIOs sind bidirektional.
- **Nicht 5-V-tolerant:** kein GPIO darf über ~3,6 V gezogen werden (relevant für 595-`/OE`, siehe 6.1).

---

## 5. ESP32-S3 Portbelegung

| GPIO | Signal | Richtung | Verbindung | Hinweis |
|------|--------|:--------:|------------|---------|
| **1**  | SD **/CS**  | OUT | → SD-Kartenleser | SPI2 |
| **2**  | SD **MOSI** | OUT | → SD-Kartenleser | SPI2 |
| **38** | SD **CLK**  | OUT | → SD-Kartenleser | SPI2 |
| **48** | SD **MISO** | IN  | ← SD-Kartenleser | SPI2 |
| **47** | I²S **LRC** (+DIP1) | OUT/IN | → MAX98357A LRC | DIP1 gemultiplext |
| **21** | I²S **BCLK** (+DIP2) | OUT/IN | → MAX98357A BCLK | DIP2 gemultiplext |
| **14** | I²S **DIN** (+DIP3) | OUT/IN | → MAX98357A DIN | DIP3 gemultiplext |
| **4**  | **ADJUST**-Taster | IN | Taster gegen GND | Bedienung |
| **5**  | **SET**-Taster | IN | Taster gegen GND | Bedienung |
| **13** | **DIP-Read** | IN | ← gemeinsamer Pol des DIP-Schalters S4 | Eingang mit Pull-up; DIP1-3 werden über 47/21/14 gescannt (Dioden, siehe 13.3) |
| **17** | SPI3 **SCLK** | OUT | → 74HCT541 (→MAX) **und** direkt → MCP | HW-SPI3-Takt |
| **8**  | SPI3 **MOSI** | OUT | → 74HCT541 (→MAX) **und** direkt → MCP | |
| **7**  | SPI3 **MISO** | IN | ← MCP23S17 SO (3,3 V) | nur MCP treibt MISO |
| **18** | **MAX7221 CS** (= LOAD-Pin) | OUT | → 74HCT541 (+Serien-R) → MAX7221 | aktiv-LOW; Latch mit steigender CS-Flanke; **R58 = 10 kΩ → 3,3 V** am 541-Eingang (siehe 11) |
| **15** | **MCP23S17 /CS** | OUT | → MCP23S17 CS (3,3 V) | **R57 = 10 kΩ → 3,3 V** (siehe 11) |
| **6**  | **MCP23S17 INT** | IN | ← MCP **INTA** (INTB bleibt offen) | 1 INT-Leitung; `IOCON.MIRROR` = 1 (siehe 9.3) |
| **3**  | **74HC595 SER** | OUT | → 74HCT541 → 595 (Daten) | dedizierte Lampen-IOs; Strapping-Pin, **10 kΩ Pulldown → GND** |
| **10** | **74HC595 SRCLK** | OUT | → 74HCT541 → 595 (Schiebetakt) | |
| **9**  | **74HC595 RCLK** | OUT | → 74HCT541 → 595 (Latch) | |
| **16** | **74HC595 /OE** | OUT | → 2N7002 → 595 `/OE` | invertiert; HIGH = Lampen an (siehe 6.1) |
| **12** | **Spule 1** | OUT | → 74HCT541 → IRL540 #1 | |
| **11** | **Spule 2** | OUT | → 74HCT541 → IRL540 #2 | |
| 0 | BOOT | — | BOOT-Taster | reserviert |
| 19/20 | USB (nativ) | — | **nicht belegt (`nc`)** | native USB-Serial/JTAG optional; frei |
| 43/44 | UART0 | OUT/IN | → **CH340C** (X3, USB-UART) | Programmierung/Konsole via USB-C |
| 39–42 | **Reserve** | — | frei | JTAG-Pins, als GPIO nutzbar (dann kein JTAG-Debug) |

**Festverdrahtete Steuerpins (kein GPIO nötig):**
- MCP23S17 `A0`, `A1`, `A2` → fest auf **GND** (Adresse `000`, siehe Abschnitt 9.1).
- MCP23S17 `/RESET` → fest auf **3,3 V** (Pull-up 10 kΩ).
- 74HC595 `/SRCLR` (Master Reset) → fest auf **5 V**.
- 74HCT541 `/OE1`, `/OE2` (Pin 1 + 19) → **GND** (Buffer immer aktiv).
- MAX98357A `SD_MODE`/`GAIN` → per Widerstand (Kanalwahl (L+R)/2, Gain nach Wunsch).

> **Bilanz:** 22 GPIOs belegt (1–18, 21, 38, 47, 48). Frei bleiben **GPIO 39–42** → **4 Reserve** am ESP (die JTAG-Pins MTCK/MTDO/MTDI/MTMS; als GPIO nutzbar, dann entfällt JTAG-Debug – die Konsole läuft ohnehin über den CH340). **IO19/20 (native USB) sind nicht belegt** → zusätzlich frei; **Programmierung/Konsole laufen über die CH340C-Brücke an UART0 (43/44)** am USB-C. BOOT (0) reserviert. Am MCP23S17 stehen **2 Reserve**-Eingänge zur Verfügung.

**Referenz-Designatoren (Schaltplan v1.0):** ESP32-S3 = `IC5` · 74HC595 = `IC1`–`IC4` ·
AMS1117-3.3 = `IC6` · 74HCT541 = `IC7` · MCP23S17 = `X1` · CH340C = `X3` · MAX7221 = `X4` ·
MAX98357A = `M1` · 2N7002 (595-`/OE`) = `T33` · IRL540 = `Q1`/`Q2` · SS24 = `D4` ·
CS-Pull-ups = `R57` (GPIO15) / `R58` (GPIO18).
*(In v07 hießen dieselben Bausteine teils anders – siehe Abschnitt 14.)*

---

## 6. 74HCT541 – Kanalbelegung (Pegelwandler 3,3 V → 5 V)

Oktal-Buffer, nicht invertierend. Eingänge 3,3-V-tauglich (HCT), Ausgänge treiben 5 V. Versorgung: **5 V**. `/OE1` (Pin 1) und `/OE2` (Pin 19) auf GND.

| Kanal | Eingang (3,3 V) von | Ausgang (5 V) nach | Funktion |
|:-----:|---------------------|--------------------|----------|
| 1 | ESP GPIO17 (SPI3-SCLK) | MAX7221 CLK | Display-Takt (Serien-R am Ausgang, siehe Abschnitt 8) |
| 2 | ESP GPIO8 (SPI3-MOSI) | MAX7221 DIN | Display-Daten (Serien-R) |
| 3 | ESP GPIO18 (CS) | MAX7221 CS | Display-Latch / Chip-Select (Serien-R) |
| 4 | ESP GPIO3 (SER) | 595 SER | Lampen-Daten |
| 5 | ESP GPIO10 (SRCLK) | 595 SRCLK | Lampen-Schiebetakt |
| 6 | ESP GPIO9 (RCLK) | 595 RCLK | Lampen-Latch |
| 7 | ESP GPIO12 | IRL540 #1 Gate | Spule 1 |
| 8 | ESP GPIO11 | IRL540 #2 Gate | Spule 2 |

Alle 8 Kanäle belegt. Die SPI3-Leitungen SCLK/MOSI gehen **zusätzlich** direkt (ungepuffert, 3,3 V) an den MCP23S17. Die Mischung aus schnellem Takt und langsamen DC-Spulensignalen in einem Baustein ist unkritisch.

### 6.1 595-`/OE` über 2N7002 (statt 9. Kanal)

Würde `/OE` ebenfalls über den 74HCT541 laufen, wären **9** Leitungen nötig → ein zweiter IC. Stattdessen ein **2N7002** (`T33`, kleiner logic-level N-MOSFET) als Open-Drain-Treiber:

```
ESP GPIO16 ──[ 100 Ω ]──┬──► Gate T33 (2N7002)   Drain ──┬──► 595 /OE
                        │                                └──[ 10k Pull-up → +5 V ]
                        └──[ 10k Pulldown → GND ]  Source ──► GND
```

- **GPIO16 HIGH** → FET leitet → `/OE` = LOW → **Lampen aktiv**.
- **GPIO16 LOW / Boot (Hi-Z)** → FET sperrt → Pull-up zieht `/OE` = 5 V → **Lampen aus**.
- Der Gate-Pulldown sorgt beim Boot (GPIO16 noch Eingang) für sicheres Sperren → **Boot-Blanking bleibt erhalten**. Er sitzt im Schaltplan v1.0 **hinter** dem 100-Ω-Serienwiderstand, also direkt am Gate-Knoten – dort, wo er wirkt. *(In v07 fehlte er, Review-Punkt 3.)*
- Der ESP-Pin sieht nie 5 V (durch den FET entkoppelt) → kein Problem mit fehlender 5-V-Toleranz.
- **PWM-Dimmen** weiter möglich (invertiertes Duty auf GPIO16); Anstiegsflanke über das RC aus Pull-up + Gate-Kapazität – für globale Lampenhelligkeit unkritisch.

> **Achtung Firmware:** Die Logik ist **invertiert** – GPIO16 = HIGH schaltet die Lampen **ein**.

---

## 7. 74HC595-Kaskade – Lampen (32 Ausgänge, 30 genutzt)

**Aufbau:** 4× 74HC595 in Reihe (QH' → SER des nächsten) = **32-Bit-Schieberegister**. Versorgung **5 V**. Jeder Ausgang treibt das Gate eines kleinen **Logic-Level-N-MOSFET** (z. B. 2N7002 / AO3400), der die zugehörige Lampe **low-side** gegen GND schaltet. Lampen-Pluspol liegt fest auf 5 V. Angesteuert über **eigene, dedizierte ESP-IOs** (nicht am SPI3-Bus).

**Signale:**
- `SER` ← 74HCT541 (GPIO3, 5 V) – Daten
- `SRCLK` ← 74HCT541 (GPIO10, 5 V) – Schiebetakt
- `RCLK` ← 74HCT541 (GPIO9, 5 V) – Übernahme ins Ausgangs-Latch
- `/OE` ← 2N7002 (GPIO16, 5 V) – LOW = Ausgänge aktiv, HIGH = alle Ausgänge hochohmig (siehe 6.1)
- `/SRCLR` → fest 5 V

**Bit-Zuordnung (Vorschlag):**

| 595 | Bit (Q) | Lampen |
|-----|---------|--------|
| IC1 (erstes im Bus) | Q0…Q7 | Lampe 1–8 |
| IC2 | Q0…Q7 | Lampe 9–16 |
| IC3 | Q0…Q7 | Lampe 17–24 |
| IC4 (letztes) | Q0…Q7 | Lampe 25–30, **Q6/Q7 = Reserve** |

> **30 von 32 Ausgängen genutzt**, 2 Reserve.

**Wichtig – definierter Aus-Zustand:**
- Jedes MOSFET-Gate braucht einen **Pulldown (z. B. 100 kΩ)** nach GND. Solange `/OE` HIGH ist (Boot) sind die 595-Ausgänge hochohmig – der Pulldown hält das Gate sicher LOW → Lampe aus.
- **5-V-Strombudget:** 30 Lampen × Lampenstrom. Bei z. B. 100 mA/Lampe = bis zu 3 A. Das 5-V-Netzteil und die Leiterbahnen entsprechend dimensionieren (siehe Abschnitt 12).

**Ansteuerung:** Da die Kaskade an eigenen IOs hängt, kann das 32-Bit-Muster jederzeit unabhängig von Display-/Kontakt-Transfers geschoben und mit `RCLK` übernommen werden (per SW-SPI / Bit-Bang – 32 Bit sind unkritisch schnell).

---

## 8. MAX7221 – Displays (8× 7-Segment, common cathode, gemultiplext)

### 8.1 Display-Architektur und Belegung des Steckers J2

Die 8 Ziffern sind **fest als gemultiplexte 8×8-Matrix** verdrahtet – nicht statisch. Das gibt der Original-Stecker (34-poliger Wannenstecker) vor; auf der Steuerplatine ist das **J2**.

> **Maßgeblich ist `datasheets/Kegelautomat_v10_SCH.PDF`.** Die Belegung wurde in Revision
> **v1.0 korrigiert**: Gegenüber v07 sind alle Signale in die jeweils **andere Pin-Reihe
> derselben Spalte** gewandert (Digits von den geraden auf die ungeraden Pins, Segmente
> umgekehrt). Die früher als GND angenommenen Gegenpins sind **`nc`** – bei der Kontrolle
> der Displayplatinen zeigte sich, dass dort **überhaupt kein GND angeschlossen** ist. Die
> „Masse-Verschachtelung", die frühere Fassungen dieser Doku forderten, gibt es in der
> Original-Verdrahtung also nicht; siehe 8.4 Punkt 6 zur Frage, ob man sie einseitig
> nachrüsten sollte.

| J2-Pin | Netz | Ziffer / Segment |
|:------:|------|------------------|
| 1  | `DIG_1` | Bip 1er |
| 3  | `DIG_5` | Bip 10er |
| 5  | `DIG_7` | Credit 1er |
| 7  | `DIG_3` | Credit 10er |
| 9  | `DIG_2` | Score 1er |
| 11 | `DIG_6` | Score 10er |
| 13 | `DIG_4` | Score 100er |
| 15 | `DIG_0` | Score 1000er |
| 20 | `SEG_DP` | dP2 |
| 22 | `SEG_G` | g |
| 24 | `SEG_F` | f |
| 26 | `SEG_E` | e |
| 28 | `SEG_D` | d |
| 30 | `SEG_C` | c |
| 32 | `SEG_B` | b |
| 34 | `SEG_A` | a |
| 2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 21, 23, 25, 27, 29, 31, 33 | — | **`nc`** (nicht beschaltet) |

- **8 Segmentleitungen** (`SEG_A`…`SEG_DP`), über alle Ziffern gemeinsam (Segment-Bus) – im Stecker in **absteigender** Reihenfolge (dP2 → a), also Pin 20 = dP2 … Pin 34 = a.
- **8 Digit-Auswahlleitungen** (`DIG_0`…`DIG_7`), je ein Common pro Ziffer. Die Zuordnung ist bewusst „verwürfelt" – für die Firmware ist die Spalte **Ziffer** maßgeblich, nicht die Pin-Nummer. Diese Zuordnung `DIG_n` → Ziffer ist **unverändert gegenüber v07**; nur die Steckerpins haben sich verschoben.
- Die 8 Ziffern liegen physisch auf **3 Platinen** (2× Bip, 2× Credit, 4× Score = **2×2 + 1×4**), alle über **ein ~1 m langes 34-poliges Flachbandkabel** angebunden.
- Segmente über alle Ziffern zusammengefasst → **klassische Multiplex-Matrix** (8 SEG + 8 DIG) = exakt die MAX7221-Topologie. Statischer Betrieb wäre für das lange Kabel elektrisch besser, ist mit dieser Verdrahtung aber **nicht** möglich (bräuchte 64 Einzel-Segmentleitungen und neue Display-Platinen).

### 8.2 Warum MAX7221 statt MAX7219 (Variante B)

Der Treiber bleibt auf der **Hauptplatine**, die 16 Matrixleitungen laufen über die ~1 m des Bands. Weil das Multiplexen über diese Länge das kritische Thema ist (**EMV** – siehe die Schleifenbetrachtung in 8.4.1), wird bewusst der **MAX7221** statt des sonst gleichwertigen MAX7219 eingesetzt:

- **Slew-rate-begrenzte Segmenttreiber** → deutlich sanftere Flanken auf den 8 langen Segmentleitungen → weniger Ringing und EMV. (Der MAX7219 treibt die Segmente ungebremst.)
- **Echtes SPI-Chip-Select:** Der MAX7221 schiebt Daten **nur bei aktivem (LOW) CS**. Damit taktet ein Kontakt-Poll des MCP23S17 den MAX **nicht** mit (kein Frame-Versatz durch Störflanken, keine dauernd sendende 1-m-„Antenne" – beim MAX7219 wäre das ein Restproblem).
- **Registerkompatibel** zum MAX7219 → **Firmware praktisch identisch**. Einziger Unterschied im Handling: CS zwischen den Frames sauber HIGH führen (macht man ohnehin).

### 8.3 Signale (alle 5 V, aus dem 74HCT541 – je mit Serien-R)

- `DIN` ← SPI3-MOSI (GPIO8)
- `CLK` ← SPI3-SCLK (GPIO17)
- `CS`  ← GPIO18 (= LOAD-Pin des Bausteins)
- `DOUT` → **nicht** an den ESP-MISO (keine Bus-Kollision mit dem MCP, siehe Abschnitt 10)

### 8.4 Maßnahmen für das ~1-m-Kabel (Variante B)

1. **Serien-R an `CLK`, `DIN`, `CS` – EMV-Vorsorge, nicht kabelbedingt:** 68–100 Ω direkt am **74HCT541-Ausgang** (Widerstand am Treiber-Pin, nicht am MAX), **auf allen drei Leitungen gleicher Wert**, damit kein Versatz zwischen Takt und Daten entsteht.
   > **Einordnung:** Diese drei Leitungen laufen in Variante B **nicht** über das Band – 541 und MAX7221 sitzen wenige Zentimeter voneinander auf der Hauptplatine. Bei ~5–8 ns Flankenzeit des 74HCT541 und ~6 ns/m auf FR4-Microstrip liegt die Grenze „elektrisch kurz" (l < t_r / 6·t_pd) bei **≈ 17 cm** – die Strecke ist also um eine Größenordnung unkritisch, Reflexionen sind hier kein Thema (Reflexionen hängen an der Flankensteilheit, nicht am Takt). Die Widerstände bleiben trotzdem bestückt: Sie kosten nichts, dämpfen Flanken und Abstrahlung, und das Timing bleibt entspannt (100 Ω gegen ~30 pF ≈ 3 ns; die MAX-Eingänge haben 1 V Hysterese). **Zwingend** werden sie erst bei **Variante A**, wo `DIN/CLK/CS` über die 1 m gehen (siehe Kasten am Ende von Abschnitt 8).
2. **SPI-Takt für den MAX auf ~1 MHz** (in ESP-IDF pro Device über `clock_speed_hz` im `spi_device_interface_config_t`). Für 8 Digits reicht das dicke; langsamere Flanken über 1 m sind ein Geschenk. Der MCP23S17 darf am selben SPI3-Bus weiter mit 8–10 MHz laufen – der Treiber schaltet die Rate pro Transaktion um.
3. **RSET moderat** wählen (12 kΩ, siehe 8.5) – primär wegen der Bauteilgrenzwerte des Digits, **nicht** wegen Ghosting.
   > **Kapazitives Ghosting ist hier quantitativ kein Thema.** Frühere Fassungen dieser Doku
   > haben es als Hauptrisiko der 1-m-Strecke geführt; die Rechnung trägt das nicht:
   > Eine DIG-Ader hat auf 1 m rund **50 pF** (ungeschirmt) bis **110 pF** (falls die
   > Nachbaradern auf GND lägen). Beim Digit-Wechsel muss diese Kapazität um ~3 V umgeladen
   > werden → **Q = C·ΔV ≈ 330 pC**. Ein Segment liefert dagegen in einer Digit-Periode
   > (156 µs bei ~800 Hz Framerate) **40 mA × 156 µs = 6,24 µC**. Das sind **vier
   > Größenordnungen** Unterschied – der Ghost läge bei ~0,005 % Helligkeit und ist
   > unsichtbar. Das gilt in beide Richtungen: Zusätzliche Kabelkapazität schadet ebenso
   > wenig (Slew einer SEG-Leitung: 100 pF × 2 V / 40 mA ≈ 5 ns).
4. **Abblockung direkt am MAX7221:** 10 µF Elko **+** 100 nF Keramik an V+/GND (die Digit-Treiber ziehen im Multiplex kräftige Spitzen – das ist die eigentliche Störquelle). **Beide GND-Pins (4 und 9) anschließen** (wird gern übersehen), durchgehende Massefläche unter den Signalleitungen.
5. **Original-„Widerstände"-Platine entfernen/überbrücken:** Der MAX7221 ist eine **Konstantstromquelle** (Strom kommt aus RSET). Serienwiderstände in den SEG-/DIG-Leitungen fressen nur die ohnehin knappe Spannungsreserve bei V+ = 5 V – dort **keine** Widerstände.
6. **Freie Adern des Bands: optional einseitig auf GND** – lohnend, aber kein Grund für einen Respin. Siehe 8.4.1.

#### 8.4.1 Die 18 freien Adern – einseitig erden oder nicht?

Die Displayplatinen haben **keinen GND-Anschluss** (an der Hardware geprüft), die 18
übrigen Adern des Bands enden dort also offen. Sie ließen sich platinenseitig auf GND
legen. Was das bringt und was nicht:

**Was es *nicht* ist – und warum die alte Begründung fiel:** Frühere Fassungen sprachen von
Masse-**Rückleitern**. Das war falsch gedacht. Der Rückstrom eines Segments fließt vom
SEG-Pin über die LED und die **DIG-Leitung** zurück zum MAX7221 – nie über GND. Eine
einseitig aufgelegte Ader führt per Definition **keinen Strom** und kann deshalb prinzipiell
kein Rückleiter sein. An der Stromschleife ändert einseitiges GND exakt **nichts**.

**Was es bringt – elektrostatische Schirmung.** Die funktioniert bei **einseitigem**
Anschluss vollständig (E-Feld-Schirme brauchen genau eine Masseverbindung; nur
Magnetfeldschirme brauchen Stromfluss). Die Geometrie ist bereits richtig: Im Flachband
liegt Ader *n* physisch neben *n ± 1*, und die v1.0-Belegung trennt **jede** Signalader
durch eine freie Ader (DIG auf 1, 3 … 15, SEG auf 20, 22 … 34). Auf GND gelegt werden diese
Adern vom Koppelpfad zum Schirm. Relevant ist das weniger für das Display selbst als für die
**Umgebung**: Im selben Gehäuse schalten 30 Lampen mit bis zu 3 A und zwei 24-V-Spulen –
gegen deren kapazitive Einstreuung wirkt der Schirm. Nebenbei bekommen 18 sonst floatende,
1 m lange Drähte ein definiertes Potenzial, statt als kleine Antennen ein- und wieder
auszukoppeln.

**Was es nicht behebt – die Stromschleife.** SEG-Block (Adern 20–34) und DIG-Block (1–15)
liegen bis zu 33 Rastermaße = bei 1,27 mm Raster **~4,2 cm** auseinander. Die Schleife
SEG → LED → DIG umfasst damit rund 1 m × 4,2 cm ≈ **0,04 m²**, das entspricht grob
**~2,4 µH**. Dagegen hilft nur, SEG- und DIG-Adern paarweise zu verschachteln – das gibt die
Original-Steckerbelegung nicht her – oder **Variante A**. Einseitiges GND ändert daran nichts.

> **Empfehlung:** Bei einer künftigen Revision mitnehmen (18 Pads an die GND-Fläche kosten
> nichts, ein messbarer Nachteil existiert nicht – siehe die Kapazitätsrechnung in Punkt 3).
> **Kein Grund für einen Respin allein deswegen**, und erst recht keine Handverdrahtung von
> 18 Pins auf der bestückten v1.0. Unkritisch ist es auch beim Stecken: Selbst ein um eine
> Position verschobener Stecker legt nur Konstantstromquellen (SEG) bzw. Stromsenken (DIG)
> auf GND – und der Wannenstecker ist ohnehin codiert.

### 8.5 Konfiguration (identisch zum MAX7219)

- **RSET** (V+ → ISET) setzt den Segment-**Spitzenstrom** (I_SEG ≈ 100 × I_ISET; **nie < 9,53 kΩ**). Für den SC08-11EWA (V_F ≈ 2 V) liefert Datenblatt-Tabelle 11 **RSET ≈ 11,8 kΩ → 40 mA** (= vom MAX empfohlenes Maximum). **Bestückungswert 12 kΩ** (0402, ~2 mW Verlust). Ein 10 kΩ träfe bei diesem niedrigen V_F ~45 mA und läge damit knapp über den 40 mA – **12 kΩ ist sauberer**. Feineinstellung der Helligkeit dann über das Intensity-Register in Software.
- **Scan-Limit-Register** = 7 (→ 8 Digits aktiv).
- **Decode-Mode**: Code-B für reine Ziffernanzeige, oder No-Decode für individuelle Segmentsteuerung (das `dP2`-Segment ist das 8. Bit pro Ziffer).
- **Intensity-Register**: digitale Helligkeit (16 Stufen).
- **Shutdown-Register**: nach Power-up ist die Anzeige geblankt – im Setup aktiv schalten.

**Hinweis Logikpegel:** Die 5-V-Ansteuerung über den 74HCT541 stellt sicher, dass die 3,5-V-V_IH-Schwelle des MAX7221 (identisch zum MAX7219) sicher überschritten wird (siehe Abschnitt 3). Ohne den Pegelwandler wäre der Betrieb mit 3,3 V außerhalb der Spezifikation.

### 8.6 Display-Baustein Kingbright SC08-11EWA

- 0,8″ Einzeldigit, **common cathode** (lt. Datenblatt bestätigt), **rechter** Dezimalpunkt, Hi-Eff-Rot (627 nm) – passt zum common-cathode-only MAX7221.
- **V_F = 1,9 V typ / 2,3 V max @ 10 mA**, **ein** LED-Chip pro Segment. Damit hat der MAX7221 bei V+ = 5 V **reichlich Spannungsreserve** (der Baustein ist bis V_LED = 3,5 V charakterisiert) – **kein Headroom-Problem**. (Das wäre nur bei großen 2-Chip-Digits mit ~4 V V_F kritisch geworden.)
- Grenzwerte je Segment: **30 mA DC, 160 mA Peak** (1/10 Duty, 0,1 ms). Bei RSET = 12 kΩ → ~40 mA Peak, im 1/8-Multiplex ≈ 5 mA Mittel je Segment: ~4× unter der Peak-, ~6× unter der DC-Grenze.
- **5-V-Strombudget Display:** eine „8." = 8 Segmente × 40 mA = **~320 mA** je aktivem Digit. Da immer nur ein Digit leuchtet (Multiplex), ist das zugleich rund der Mittelwert im Worst Case (alle Digits „8."). Für das 5-V-Netzteil ~350 mA für die Anzeige einplanen (zusätzlich zu Lampen/Audio).
- Datenblatt: Kingbright SC08-11EWA, Spec DSAP8389 Rev V.1A (2020).

> **Restrisiko & Rückfallebene:** Das Datenblatt (S. 10) empfiehlt ausdrücklich das Gegenteil von Variante B: *„The MAX7219/MAX7221 should be placed in close proximity to the LED display, and connections should be kept as short as possible to minimize the effects of **wiring inductance** and electro-magnetic interference."* Das eigentliche Restrisiko ist damit die **Leitungsinduktivität der Matrixschleife** (~0,04 m² Schleifenfläche, ~2,4 µH – siehe 8.4.1), nicht das oft zitierte kapazitive Ghosting: Das ist bei diesen Strömen rechnerisch vier Größenordnungen zu klein, um sichtbar zu werden (8.4 Punkt 3). Die Induktivität wirkt sich vor allem als **Abstrahlung** aus – die 320-mA-Digit-Pulse laufen über eine 1 m lange, weit aufgespannte Schleife –, für die Anzeigefunktion selbst ist sie unkritisch (L/R ≈ 50 ns gegen 156 µs Digit-Periode). Zeigt sich im Betrieb dennoch ein Problem (Rest-Ghosting, oder Störungen anderer Baugruppen im Takt der Anzeige), ist die saubere Lösung (**Variante A**) den MAX7221 auf eine kleine Platine **ins Display-Gehäuse** zu setzen – dann laufen nur noch `DIN/CLK/CS/5 V/GND` über die 1 m, die 16 Matrixleitungen bleiben kurz und die 320-mA-Digit-Pulse bleiben beim Display. Die Firmware bliebe unverändert.
>
> **Was bei Variante A mitwandert:** `RSET` und vor allem die Abblockung (10 µF + 100 nF, laut Datenblatt „as close to the device as possible") gehören dann auf die Display-Platine, ebenso beide GND-Pins. Und **erst dann** sind die Serien-R (68–100 Ω) in `CLK/DIN/CS` elektrisch wirklich nötig: Über 1 m ist die Leitung elektrisch lang, unterminiert gibt es Ringing. Der Wellenwiderstand eines Flachbands liegt je nach Masseführung grob bei ~100–150 Ω (ohne definierten Rückleiter – wie hier, siehe 8.1 – eher am oberen Ende und schlechter definiert), der 74HCT541 bei ~40 Ω Ausgangsimpedanz → 68–100 Ω sind eine brauchbare Dämpfung. **Bei Variante A entfällt das Problem aus 8.4.1 von selbst:** Über das Band gehen dann nur noch 5 Signale, und dort ist GND ein *echter* Rückleiter (die Display-Platine bekommt ja 5 V/GND). Man kann `CLK/DIN/CS` dann beidseitig zwischen Masseadern legen – im 34-poligen Band ist reichlich Platz –, und das wirkt dann auch gegen die magnetische Kopplung, nicht nur elektrostatisch.

---

## 9. MCP23S17 – Kontakte (16 Eingänge)

SPI-Port-Expander mit **16 IO** (Port A: GPA0–7, Port B: GPB0–7). Versorgung **3,3 V** (damit 3,3-V-SPI direkt funktioniert, siehe Abschnitt 3). SPI-Signale (SCK, SI=MOSI, SO=MISO, CS) **ungepuffert** direkt zum ESP am **SPI3-Bus**. Alle 16 IO dienen als **Kontakt-Eingänge**; die Spulen hängen direkt am ESP (Leistungsteil siehe Abschnitt 9.4).

### 9.1 Adresspins A0/A1/A2 → alle drei fest auf GND

Die drei Adresspins sind Eingänge und **dürfen nie floaten** – das Datenblatt verlangt die externe Beschaltung ausdrücklich unabhängig vom Zustand des HAEN-Bits: *„The address pins (A2, A1 and A0) must be externally biased, regardless of the HAEN bit value."*

**GND** ist hier die richtige Wahl, weil die Geräteadresse damit in **beiden** HAEN-Fällen `000` ist:

| `IOCON.HAEN` | Verhalten laut Datenblatt | Adresse bei A2/A1/A0 = GND |
|--------------|---------------------------|----------------------------|
| **0** (POR-Default) | Adresspins deaktiviert, Adresse fest `A2 = A1 = A0 = 0` | `000` |
| **1** | Adresse folgt dem Pin-Zustand | `000` |

→ Der Opcode `0x40` (write) / `0x41` (read) stimmt dadurch immer, egal ob eine Lib das HAEN-Bit setzt oder nicht. Bei Beschaltung auf 3,3 V hinge der korrekte Opcode dagegen am HAEN-Bit – eine vermeidbare Fehlerquelle. Da nur **ein** MCP am Bus hängt und ein eigenes `/CS` (GPIO15) besitzt, wird die Hardware-Adressierung ohnehin nicht gebraucht.

Direkte Verbindung nach GND genügt (keine Widerstände nötig). Wer sich einen zweiten MCP am selben `/CS` offenhalten will, sieht je Pin einen Lötjumper (Pad → GND / Pad → 3,3 V) vor.

### 9.2 IO-Belegung

Maßgeblich ist `datasheets/Kegelautomat_Steckerbelegung.xlsx` (Blatt „Stecker", Stecker
**J3** und **J4**). Alle Eingänge mit internem Pull-up, jeder Kontakt schaltet gegen GND.

| Port | Schalter | Stecker/Pin | Funktion |
|------|----------|-------------|----------|
| GPB0 | SW1  | J3-12 | Kontakt 1 |
| GPB1 | SW2  | J3-11 | Kontakt 3 |
| GPB2 | SW3  | J3-10 | Kontakt 5 |
| GPB3 | SW4  | J3-9  | Kontakt 7 |
| GPB4 | SW5  | J3-8  | Kontakt 9 |
| GPB5 | SW6  | J3-7  | Kontakt 8 |
| GPB6 | SW7  | J3-6  | Kontakt 6 |
| GPB7 | SW14 | J3-5  | **Reserve** (auf den Header geführt, nutzbar) |
| GPA1 | SW8  | J3-4  | Kontakt 4 |
| GPA0 | SW9  | J3-3  | Kontakt 2 |
| GPA2 | SW10 | J4-6  | Start ⚠ |
| GPA3 | SW11 | J4-4  | Münzer NO ⚠ |
| GPA4 | SW12 | J4-3  | Münzer NC ⚠ |
| GPA5 | SW13 | J4-2  | SLAM-Kontakt (NO) ⚠ |
| GPA6, GPA7 | – | – | **Reserve**, `nc` (nicht auf Header geführt) |

> **Bilanz:** **13 belegte Kontakte** (Kontakt 1–9, Start, Münzer NO/NC, SLAM)
> + **1 Reserve auf dem Header** (SW14, GPB7) + **2 Reserve `nc`** (GPA6/7) = 16 IO.

⚠ **Noch zu verifizieren:** Die Steckerbelegung nennt für J4 (SW10–SW13) *keine*
Port-Zuordnung. Dass diese vier auf **GPA2–GPA5** liegen, ist zwingend (alle anderen
IO sind belegt bzw. `nc`) – **offen ist nur die Reihenfolge innerhalb GPA2–GPA5.**
Beim Bestücken gegen den Schaltplan prüfen und hier wie in der Firmware
(`main/hwmap.h`, Tabelle `contact_map[]`) eintragen.

Die Kegel-/Zahl-/Kontakt-Reihenfolge auf dem Spielfeld ist von links nach rechts
**1, 3, 5, 7, 9, 8, 6, 4, 2** (Blatt „Spielfeld").

### 9.3 Kontakt-Erfassung per Interrupt

- Alle Eingänge mit **internem Pull-up** (`GPPU` = 1). Ein geschlossener Kontakt zieht den Pin auf GND.
- **Interrupt-on-Change** (`GPINTEN` = 1, Vergleich gegen Vorwert) meldet jede Kontaktänderung.
- **INTA/INTB werden per `IOCON.MIRROR` = 1 gespiegelt** – die Verodung passiert **intern**: *„the INTn pins are functionally OR'ed so that an interrupt on either port will cause both pins to activate."* Beide Pins führen also dasselbe Signal.
- **Verdrahtet wird nur `INTA` → GPIO6; `INTB` bleibt offen.** Ein Draht zwischen beiden Pins bringt funktional nichts und ist elektrisch schlechter: Mit dem Default `IOCON.ODR` = 0 sind die INT-Pins **Push-Pull**-Ausgänge (*„Active driver output"*). Parallelgeschaltet hinge die Kollisionsfreiheit allein am gesetzten MIRROR-Bit – der POR-Default von IOCON ist aber `0x00`, also MIRROR = 0 (Pins getrennt). Ein echtes Wire-OR bräuchte `ODR` = 1 (Open-Drain, überschreibt INTPOL) + Pull-up 10 kΩ nach 3,3 V; hier nicht nötig.
- INTA treibt push-pull und ist mit `INTPOL` = 0 **aktiv-LOW** → am ESP ist **kein externer Pull-up** erforderlich.
- Der ISR liest nach der INT-Flanke an **GPIO6** per SPI `INTF`/`INTCAP`/`GPIO` beider Ports und ermittelt den geänderten Kontakt.

### 9.4 Spulen-Leistungsteil (IRL540, direkt vom ESP)

**Funktion der Münz-Weiche:** Die Spulen stellen beim Münzeinwurf die Weiche. Im **stromlosen („Aus"-)Zustand fallen die Münzen durch** – der Automat nimmt kein Geld an. Erst die bestromte Spule leitet die Münze in den Annahmeweg. Das macht „Spule aus" zum sicheren Grundzustand (siehe Abschnitt 11).

- **ESP-Ausgang** (GPIO12/11) HIGH → 74HCT541 (5 V) → **IRL540-Gate** (100 Ω Serien-R) → Spule (24 V) low-side eingeschaltet.
- **Gate-Pulldown 10 kΩ** je IRL540 (`Q1`/`Q2`) → definierter Aus-Zustand bei Boot / vor Firmware-Init (die ESP-Ausgänge sind vor der Init hochohmige Eingänge).

**24-V-Zweig und Freilaufdioden liegen außerhalb dieser Platine** (Review-Punkte 1 und 2, so gelöst ab v1.0):

- Die **24-V-Versorgung** kommt von einer **separaten Platine**; auf der Steuerplatine existiert kein 24-V-Netz. Deren GND muss mit dem Platinen-GND verbunden sein, sonst hat der IRL540 keinen Rückweg.
- Die **Freilaufdiode** sitzt **direkt an der Spule** (Kathode an +24 V, Anode an den Drain-Anschluss) – dort, wo sie die Abschaltspitze am kürzesten kurzschließt. Ohne sie sieht der IRL540 Spitzen ≫ 100 V (V_DS max).
- ⚠ **Achtung beim Verdrahten:** Die Klemmen **SP1/SP2 führen an *beiden* Polen dasselbe Netz** – nämlich den **IRL540-Drain** (`D_2` bzw. `D_3`). Sie sind **kein** Spulenanschluss mit +24 V und Drain, sondern nur ein doppelt herausgeführter Drain. Die +24-V-Seite der Spule wird an der externen 24-V-Platine angeschlossen, **nicht** an SP1/SP2.

---

## 10. Bus-Betrieb

Alle SPI-Bausteine arbeiten im **SPI-Mode 0** (CPOL=0, CPHA=0).

**SPI2 – SD-Karte (eigener Host):** dediziert, damit das Lesen der Audio-Files das Display-/Kontakt-Timing nicht blockiert. Eigene MISO (GPIO48), CS (GPIO1).

**SPI3 – MAX7221 + MCP23S17 (gemeinsamer Host):** SCLK (GPIO17) und MOSI (GPIO8) sind geteilt; je eigene Auswahlleitung:
- **MCP23S17**: echtes `/CS` (GPIO15). Reagiert nur bei aktivem CS.
- **MAX7221**: `CS` (GPIO18). **Echtes Chip-Select** – schiebt **nur bei aktivem (LOW) CS** und übernimmt mit der steigenden CS-Flanke. Anders als der MAX7219 wird der MAX also von einem MCP-Kontakt-Poll **nicht** mitgetaktet → kein Frame-Versatz, keine Dauer-Aussendung auf dem 1-m-Kabel. Der MAX läuft mit ~1 MHz (langsamer als der MCP), der Treiber stellt die Rate pro Transaktion um.
- **MISO** (GPIO7): nur der MCP23S17 treibt MISO (und nur bei aktivem CS). Der MAX7221-`DOUT` wird **nicht** auf den ESP-MISO geführt → keine Bus-Kollision.

**Lampen (74HC595) – eigene IOs:** vollständig vom SPI3-Bus entkoppelt. Es wird **kein** Fremd-Takt in die Kaskade eingeschoben; ein `RCLK`-Puls nach dem vollständigen 32-Bit-Frame macht das Muster sichtbar.

**I²S – MAX98357A:** separater Peripherie-Block (LRC/BCLK/DIN), unabhängig von SPI.

---

## 11. Boot- & Sicherheitsverhalten

Definierte, ungefährliche Zustände von Power-on bis Firmware-Init:

| Element | Maßnahme | Zustand beim Boot |
|---------|----------|-------------------|
| Lampen | 2N7002 (`T33`) sperrt (10 kΩ Gate-Pulldown) → 595-`/OE` per Pull-up auf 5 V → Ausgänge hochohmig + Gate-Pulldowns | **alle aus** |
| Spulen | IRL540-Gate-Pulldowns; ESP-Ausgänge (12/11) nach Reset hochohmig | **alle aus** → Münzen fallen durch (sicherer Zustand, siehe 9.4) |
| Display | MAX7221 startet im Shutdown (Datasheet) | **dunkel** |
| MCP | `/RESET` fest auf 3,3 V; Register-Defaults = alle Pins Eingang | keine ungewollten Ausgänge |
| **SPI3-`CS`** | **`R57`/`R58` je 10 kΩ → 3,3 V** an GPIO15 (MCP) bzw. am 541-**Eingang** von GPIO18 (MAX) | **sicher HIGH** → kein Baustein schiebt ein |
| Sound | I²S-Pins vor Init hochohmig; MAX98357A liefert ohne Takt kein Signal | **still** |
| Strapping | GPIO0/3/45/46 in definiertem Zustand; GPIO3 (`SER`) mit 10 kΩ Pulldown | normaler Flash-Boot |

> **Historie: die SPI3-`CS`-Lücke – gefunden 2026-07-28, in v1.0 behoben (Review-Punkt 8).**
> Bis v07 hatten die beiden Chip-Selects als einzige Signale **keinen** definierten
> Boot-Zustand: GPIO18 (MAX7221-`CS`) und GPIO15 (MCP23S17-`CS`) sind bis `spibus_init()`
> hochohmig, der 74HCT541 (Kanal 3) machte daraus einen beliebigen 5-V-Pegel. Lag `CS` dabei
> LOW, schob der MAX7221 Störflanken von `CLK`/`DIN` als Frame ein und übernahm sie mit der
> nächsten steigenden CS-Flanke.
>
> **Symptom am realen Aufbau:** Nach dem Einschalten leuchteten alle 8 Ziffern kurz hell als
> `8` – die Signatur des **Display-Test-Registers**, das laut Datenblatt *„all controls and
> digit registers (including the shutdown register)"* übersteuert und bis zum Zurückschreiben
> aktiv bleibt. Der POR-Zustand schied als Erklärung aus (dort: geblankt, Shutdown,
> Scan-Limit 1 Digit, Intensity **Minimum**). Beim MCP23S17 wog derselbe Punkt schwerer: ein
> Zufallsframe hätte `IODIR` umstellen und Eingänge zu Ausgängen machen können, die gegen die
> Kontaktschalter treiben.
>
> **Behoben in Revision v1.0** durch `R57`/`R58` (je 10 kΩ nach **3,3 V**, nie 5 V – der S3 ist
> nicht 5-V-tolerant), beide auf der ESP-Seite: `R58` am 541-**Eingang** von GPIO18 (am
> 541-*Ausgang* wäre er wirkungslos, der treibt bei `/OE` = GND aktiv), `R57` direkt am Netz
> GPIO15, das ungepuffert zum MCP läuft. `CLK`/`DIN` (GPIO17/8) floaten beim Boot weiterhin,
> brauchen aber keinen Pull-up – solange `CS` sicher HIGH liegt, schiebt keiner der beiden
> Bausteine etwas ein.
>
> **Firmwareseitig zusätzlich abgesichert:** `spibus_park_cs()` legt beide CS-Leitungen als
> allererstes in `app_main()` auf HIGH; die Display-Initialisierung läuft vor der SD-Karte.
> Das deckt das Fenster ab `app_main()`, die Pull-ups zusätzlich die Bootloader-Zeit davor.

---

## 12. Offene Punkte & Empfehlungen

**Noch offen:**

1. **5-V-Netzteil dimensionieren** – bestimmt durch den Summenstrom der 30 Lampen (Lampenstrom messen) plus MAX98357A-Spitzen. Reserve einplanen. Dazu die **Ampacity** prüfen: je Lampen-Header nur **ein** +5-V-Pin (J7 versorgt ~10 Lampen), und J1-Pin4 trägt den gesamten Board-Strom über einen einzelnen 2,54-mm-Pin.
2. **IRL540 real prüfen:** Spulenstrom messen und gegen die Transfer-Kennlinie bei V_GS = 5 V gegenchecken. Bei hohen Strömen ggf. auf einen MOSFET mit niedrigerem R_DS(on) bei 5 V wechseln.
3. **Kontakte physisch:** Pin-Zuordnung liegt in `datasheets/Kegelautomat_Steckerbelegung.xlsx` (Blatt „Stecker") und ist in Abschnitt 9.2 übernommen. Offen bleibt nur die Reihenfolge von SW10–SW13 innerhalb GPA2–GPA5 (siehe 9.2).
4. **Display-Band im Betrieb beobachten:** Die Matrixschleife über die ~1 m spannt ~0,04 m² auf (~2,4 µH, siehe 8.4.1). Zu erwarten ist keine Anzeigestörung, sondern allenfalls **Abstrahlung** im Takt der Anzeige – also eher Störungen *anderer* Baugruppen als des Displays. Rückfallebene bleibt Variante A (Kasten am Ende von Abschnitt 8). Kapazitives Ghosting ist rechnerisch ausgeschlossen (8.4 Punkt 3).
5. **Optional bei einer künftigen Revision:** die 18 freien Adern von J2 platinenseitig auf GND legen – elektrostatischer Schirm, kostet nichts, behebt die Schleife aber nicht. Abwägung in **8.4.1**.

**Dauerhaft zu beachten (kein offener Punkt, sondern Betriebsregel):**

6. **Entkopplung:** je IC 100 nF nahe an VCC/VDD; zusätzlich Elkos am 5-V-Rail. Getrennte GND-Führung (Leistungs-GND der Lampen/Spulen **und** Audio-GND sternförmig zum Logik-GND). Zur Entkopplung von USB-VBUS gegen das externe 5-V-Netzteil (D4/SS24) siehe **Abschnitt 3.3**.
7. **Reserve:** am ESP **4** (GPIO 39–42, JTAG-Pins – als GPIO nutzbar; zusätzlich IO19/20, siehe Abschnitt 5), am MCP23S17 **2** Eingänge frei (GPA6/7, `nc`).
8. **DIP-Multiplexing:** DIP1-3 liegen auf den I²S-Leitungen (47/21/14), gelesen über GPIO13. Die DIPs nur einlesen, wenn I²S ruht (beim Start). Die Entkopplung übernehmen **drei Dioden** am DIP-Schalter (siehe 13.3) – kein Tri-State-Puffer; der frühere Punkt „DIP-Puffer an 3,3 V betreiben" ist damit gegenstandslos, die Lösung ist inhärent 3,3-V-sicher.
9. **595-`/OE` invertiert:** Firmware beachten – GPIO16 = HIGH schaltet Lampen ein (2N7002, siehe 6.1).
10. **Audio:** MAX98357A-Modul – nur verifizieren, dass `SD_MODE` nicht auf 0 V (Shutdown) hängt und die Kanalwahl (L+R)/2 stimmt; Details in Abschnitt 13.1.
11. **Display-Kabel (Variante B):** Serien-R (68–100 Ω) an CLK/DIN/CS, SPI-Takt des MAX auf ~1 MHz, Abblockung + beide GND-Pins am MAX7221, RSET moderat, Original-„Widerstände"-Platine entfernen (Konstantstromquelle). Siehe Abschnitt 8.
12. **Common cathode:** Für den gewählten **SC08-11EWA lt. Datenblatt bestätigt** (Common Cathode, rechter DP) – passt zum MAX7221 (common-cathode-only). Bei abweichenden Digit-Typen vorher gegenprüfen. RSET = **12 kΩ** (nicht 10 kΩ) für ~40 mA bei V_F ≈ 2 V (siehe Abschnitt 8.5/8.6).

**In Revision v1.0 erledigt** (Details in Abschnitt 14 und `Review_v10.md`): Freilaufdioden an den Spulen · 24-V-Versorgung · 2N7002-Gate-Pulldown · USB-VBUS-Entkopplung · SPI3-`CS`-Pull-ups · GPIO3-Pulldown · Belegung des Displaysteckers J2.

---

## 13. Soundausgabe (MAX98357A über I²S, Files von SD-Karte)

Der Ton liegt als **echte Audio-Datei auf SD-Karte**, wird vom ESP32-S3 gestreamt und über **I²S** an einen **MAX98357A** (Class-D-Mono-Verstärker) ausgegeben. Der MAX98357A treibt den Lautsprecher direkt – ein zusätzlicher Analog-Verstärker ist **nicht** nötig.

**Signalkette:**

```
SD-Karte (WAV) ──SPI2──► ESP32-S3 ──I²S (LRC/BCLK/DIN)──► MAX98357A ──► Lautsprecher (4–8 Ω)
```

### 13.1 MAX98357A (I²S)

- **Eingänge:** `LRC` (Word-Select, GPIO47), `BCLK` (Bit-Clock, GPIO21), `DIN` (Daten, GPIO14).
- **Versorgung:** 2,5–5,5 V; für mehr Ausgangsleistung am **5-V-Rail** betreiben (bis ~3 W an 4 Ω).
- **Ausgang:** Brücken-Endstufe direkt an den Lautsprecher – **kein** Koppelkondensator nötig.
- **`SD_MODE`-Pin (Doppelfunktion Shutdown *und* Kanalwahl):** wird über die
  **Analogspannung** am Pin ausgewertet (interner 100-kΩ-Pulldown nach GND, externer
  Widerstand nach VDD stellt die Spannung ein). **Kein Digitalsignal.**

  | Spannung an `SD_MODE` | Ergebnis |
  |---|---|
  | < 0,16 V (Pin offen → interner Pulldown zieht auf 0 V) | **Shutdown → kein Ton** |
  | 0,16 – 0,77 V | **(L + R) / 2** — Stereo-Mix auf Mono ← **für uns richtig** |
  | 0,77 – 1,4 V | nur **rechter** Kanal |
  | > 1,4 V | nur **linker** Kanal |

  → Ziel: Spannung im **(L+R)/2-Fenster** halten, damit der Amp *an* ist und der
  gesamte Datei-Inhalt (nicht nur ein Kanal) auf den Mono-Lautsprecher geht.
  Fertige Breakouts (Adafruit & Clones) haben den passenden Widerstand meist schon
  bestückt → **am M2-Modul nur verifizieren**, dass `SD_MODE` nicht auf 0 V (Shutdown)
  gezogen ist. **Nicht** an einen ESP-GPIO legen, um Mute zu schalten: 3,3 V > 1,4 V
  ⇒ linker Kanal statt (L+R)/2. Stille stattdessen über den I²S-Strom (kein Takt /
  Nullen) erzeugen, siehe Boot-/Sicherheitstabelle Abschnitt 12.
- **`GAIN`-Pin (Grundverstärkung, unkritisch — nur Lautstärke):** ebenfalls per
  Beschaltung gewählt.

  | `GAIN`-Beschaltung | Verstärkung |
  |---|---|
  | 100 kΩ nach VDD | 3 dB |
  | direkt an VDD | 6 dB |
  | **offen (floating)** | **9 dB (Default)** |
  | direkt an GND | 12 dB |
  | 100 kΩ nach GND | 15 dB |

  → im Zweifel offen lassen (9 dB); nur ändern, falls die Grundlautstärke am
  Lautsprecher nicht passt.

### 13.2 SD-Kartenleser (SPI2)

- **SPI-Signale:** `MISO` = GPIO48, `MOSI` = GPIO2, `CLK` = GPIO38, `CS` = GPIO1.
- Eigener SPI-Host (SPI2), damit das Nachladen der Audio-Daten das Display-/Kontakt-Timing (SPI3) nicht bremst.
- Dateisystem FAT; die Effekt-/Sound-Files (WAV) liegen als Dateien auf der Karte und lassen sich ohne Re-Flash austauschen.

### 13.3 DIP-Schalter (gemultiplext, Diodenentkopplung)

Drei DIP-Schalter (DIP1-3, Codierschalter `S4` SD03) teilen sich die I²S-Leitungen (GPIO47/21/14). Statt eines Tri-State-Puffers – so stand es in früheren Fassungen dieser Doku – setzt die Platine **drei Entkopplungsdioden** ein (`D1`–`D3`, 1N4148W):

```
ESP-Pin (LRC/BCLK/DIN) ◄──|◄── DIP-Schalter S4 ──► gemeinsamer Pol = GPIO13 (READ_DIP)
                    Kathode    Anode
```

**Gelesen wird im Scan-Verfahren** (so implementiert in `main/buttons.c` des Firmware-Repos):

- **GPIO13 = Eingang mit internem Pull-up.** Die drei I²S-Pins werden **Ausgänge** und
  nacheinander **einzeln auf LOW** gezogen, die übrigen bleiben HIGH.
- Ist der zugehörige DIP **geschlossen**, fließt Strom vom Pull-up an GPIO13 über den
  Schalter und die Diode (Anode → Kathode) in den LOW getriebenen Pin – **GPIO13 liest LOW**.
  Offener DIP → GPIO13 bleibt HIGH. Also: **0 = ein, 1 = offen**.
- Die Diodenrichtung ist dabei entscheidend: Sie verhindert, dass die zwei *nicht*
  abgefragten Leitungen (HIGH) das Ergebnis verfälschen – nur der LOW getriebene Pin kann
  Strom aufnehmen.

**Im Audio-Betrieb** werden alle vier Pins zurückgesetzt (`gpio_reset_pin()`); GPIO13 ist
dann hochohmig ohne Pull-up. Die Dioden sperren in Richtung Schalter, sobald die I²S-Leitung
High führt – die Audio-Leitungen werden **nicht** belastet, und der DIP-Zweig kann nichts in
sie einspeisen.

- **Inhärent 3,3-V-sicher:** Am gesamten Zweig sind nur ESP-Pins und die Dioden beteiligt, kein 5-V-Baustein. Die frühere Anforderung „DIP-Puffer muss an 3,3 V hängen" entfällt damit ersatzlos.
- Die DIPs deshalb **beim Start lesen, bevor der I²S-Kanal angelegt wird**.

**Firmware:** Die Audiokette ist umgesetzt – WAV-Dateien (PCM, 16 Bit) von SD über den
ESP-IDF-Treiber `i2s_std` an den MAX98357A, siehe `main/audio.c` im Firmware-Repository
(`C:\Users\bonta\ESP32_source\kegelautomat`). Die DIP-Schalter werden dort beim Start
gelesen, *bevor* der I²S-Kanal angelegt wird.

**Für die Platine:** SD-Slot, MAX98357A-Modul und Lautsprecher vorsehen; Analog-/Audio-GND sternförmig und getrennt vom Leistungs-GND der Lampen/Spulen führen.

---

## 14. Platinenrevisionen

### v1.0 (2026-07-29) – aktuell

Behebt die HW-Punkte aus `Review_v07.md`; die Verifikation an Schaltplan und Stückliste steht
in `Review_v10.md`. **141 Bauteile, 34 Gruppen** (`datasheets/iBOM_Kegelautomat_v10.html`).
**Keine GPIO-Änderung** – die Firmware ist nicht betroffen.

| Änderung | Umsetzung | Doku |
|---|---|---|
| Spulen-Freilaufdioden | **extern**, direkt an den Spulen (Kathode +24 V, Anode Drain) | 9.4 |
| 24-V-Versorgung | **separate Platine**; auf der Steuerplatine kein 24-V-Netz. SP1/SP2 führen an beiden Polen den Drain | 9.4 |
| 2N7002-Gate-Pulldown | 10 kΩ Gate (`T33`) → GND, hinter dem 100-Ω-Serien-R | 6.1 |
| USB-VBUS-Entkopplung | `D4` (SS24) in Reihe: VBUS → `F1` → `D4` → +5 V | 3.2 |
| SPI3-`CS`-Pull-ups | `R57` 10 kΩ → 3,3 V an GPIO15, `R58` 10 kΩ → 3,3 V am 541-Eingang GPIO18 | 5, 11 |
| GPIO3-Pulldown | 10 kΩ an `HC595_SER_PIN` → GND (Strapping + `SER`-Eingang definiert) | 4, 5 |
| Displaystecker **J2** | Belegung korrigiert: Signale in die jeweils andere Pin-Reihe; die früheren GND-Pins sind `nc` | 8.1 |

Elektrisch neu bestückt sind damit genau **fünf** Bauteile: `R55`, `R56`, `R57`, `R58`
(je 10 kΩ) und `D4` (SS24) – der Stücklisten-Vergleich v07 → v1.0 zeigt sonst keine
Mengenänderung.

**Umnummerierte Referenz-Designatoren** (v07 → v1.0), wichtig beim Vergleich alter Notizen:

| Baustein | v07 | v1.0 |
|---|---|---|
| 74HC595 (Kaskade) | IC1, IC2, IC4, IC5 | **IC1–IC4** |
| ESP32-S3-WROOM-1 | IC6 | **IC5** |
| AMS1117-3.3 | IC7 | **IC6** |
| 74HCT541 | IC14 | **IC7** |
| MAX7221 | X5 | **X4** |
| MAX98357A | M2 | **M1** |
| MCP23S17 / CH340C / 2N7002 | X1 / X3 / T33 | unverändert |

### v07 (2026-07-17) – Prototyp, abgelöst

Erste bestückte Platine. Review und gefundene Fehler: `Review_v07.md`.
Schaltplan/Layout: `datasheets/Kegelautomat_v07_SCH.PDF`, `_PCB.PDF`,
Stückliste `datasheets/iBOM_Kegelautomat_v07.html`.

---

## Anhang: Verwendete Quellen

- `datasheets/MCP23S17_MIC.pdf` – DC-Kennwerte (V_IH = 0,8·VDD; Betrieb 1,8–5,5 V; max. 25 mA/Pin); Pin-Beschreibung + Abschnitt 1.6.6: Adresspins extern beschalten unabhängig von HAEN, HAEN = 0 → Adresse `000`; MIRROR-Bit verodert INTA/INTB intern; ODR-Default = Push-Pull
- `datasheets/max7219-max7221.pdf` – Electrical Characteristics (V_IH = 3,5 V @ V+ = 5 V; common cathode; 8 Digits). **MAX7221** hier gewählt: echtes SPI-CS + slew-rate-limitierte Segmenttreiber (EMV), sonst registerkompatibel zum MAX7219.
- `datasheets/Kegelautomat_v10_SCH.PDF`, `_PCB.PDF`, `iBOM_Kegelautomat_v10.html` – **maßgeblicher Schaltplan/Layout/Stückliste (Revision v1.0)**; Quelle für Portbelegung, J2-Displaystecker (Abschnitt 8.1) und alle Bauteil-Designatoren
- `datasheets/Kegelautomat_Steckerbelegung.xlsx` – Lampen-/Kontakt-/Digit-Zuordnung (Blätter „Stecker" und „Spielfeld")
- `datasheets/esp32-S3-pinout.pdf` – Pinout ESP32-S3-WROOM-1
- **MAX98357A** (Maxim/Analog Devices) – I²S-Class-D-Amp; Gain/Kanal per Widerstand (Herstellerdatenblatt, nicht mitgeliefert)
- `datasheets/PCB_Skizze.jpg`, `datasheets/Kegelautomat.jpg` – Layout-Skizze & Automat
- Espressif ESP32-S3 GPIO/Strapping-Doku – Strapping GPIO0/3/45/46, Flash 26–32, Octal-PSRAM 33–37 (N16R8), USB 19/20, UART0 43/44
