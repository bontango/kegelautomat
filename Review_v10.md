# Review Platine v1.0

**Geprüft am:** 2026-07-29
**Grundlage:** `datasheets/Kegelautomat_v10_SCH.PDF` (Stand 29.07.2026 12:22), `_PCB.PDF`,
`iBOM_Kegelautomat_v10.html` (29.07.2026 13:01) – **141 Bauteile, 34 Gruppen**
**Abgeglichen gegen:** `Review_v07.md`, `Steuerplatine_Doku.md`, `gpiodefs.h`, Schaltplan v07
**Vorgänger:** `Review_v07.md` (Prototyp v07) – dessen Punkte 1–8 sind hiermit erledigt.

Prüfmethode: Netzliste des v10-Schaltplans auf Pin-/Netz-Ebene aus dem PDF rekonstruiert,
gegen die v07-Netzliste gedifft und die kritischen Stellen zusätzlich als Ausschnitt visuell
kontrolliert. Das PCB wurde nur visuell aus dem Raster-PDF beurteilt (keine echte DRC).

---

## Kurzfazit

**Alle acht Punkte aus `Review_v07.md` sind erledigt** – fünf davon auf der Platine, zwei
durch externe Baugruppen, der Rest in der Doku. Zusätzlich ist der bislang nur empfohlene
GPIO3-Pulldown bestückt und die Belegung des Displaysteckers J2 korrigiert.
**Keine GPIO-Änderung** → die Firmware ist nicht betroffen.

Neu zu beachten: Am Displaystecker gibt es **keine Masse-Rückleiter mehr** (alle Gegenpins
`nc`). Das ist so gewollt, macht das Band aber empfindlicher gegen Übersprechen – siehe
„Offene Punkte".

---

## ✅ Verifiziert & korrekt

### Punkt 3 – 2N7002-Gate-Pulldown (war HW-Fehler)

`HC595_OE_PIN` (GPIO16) → **100 Ω** → Gate von **T33** (2N7002); vom Gate-Knoten **10 kΩ nach
GND**. Der Pulldown sitzt **hinter** dem Serienwiderstand, also direkt am Gate – dort, wo er
wirkt. Drain → `HC595_OE_PIN5V` mit **10 kΩ Pull-up nach +5 V**, Source an GND.

→ Beim Boot (GPIO16 hochohmig) sperrt der FET sicher, `/OE` liegt über den Pull-up auf 5 V,
die 595-Ausgänge sind hochohmig. **Boot-Blanking ist damit erzwungen**, nicht nur
wahrscheinlich.

### Punkt 4 – USB-VBUS ↔ +5 V entkoppelt (war HW-Verbesserung)

Kette: `USB-C VBUS` → **F1** (MF-MSMF150-2, PTC, I_hold 1,5 A) → **D4** (SS24) → **+5-V-Rail**.
Orientierung im Schaltplan: **Anode an der F1-Seite, Kathode am +5-V-Rail**.

→ USB kann das Board speisen, das Board aber **nicht** in den PC-Port zurückspeisen. Damit ist
gleichzeitiger Betrieb von USB und externem 5-V-Netzteil an J1 unkritisch.

### Punkt 8 – SPI3-`CS` mit definiertem Boot-Zustand (war HW-Fehler, an der bestückten v07 gefunden)

- **R57 = 10 kΩ von +3V3 an `MCP23S17_CS_PIN`** (GPIO15) – das Netz, das ungepuffert zum MCP23S17 läuft.
- **R58 = 10 kΩ von +3V3 an `MAX7221_CS_PIN`** (GPIO18) – am **74HCT541-Eingang**, nicht am
  Ausgang `MAX7221_CS_PIN5V`. Genau richtig: am Ausgang wäre der Pull-up wirkungslos, weil der
  541 bei `/OE` = GND aktiv treibt.

Beide auf **3,3 V** (nicht 5 V – der S3 ist nicht 5-V-tolerant). Damit liegen beide
Chip-Selects schon ab Power-on sicher HIGH; der beim Einschalten beobachtete Segmentblitz
(Display-Test-Register durch einen Störframe) kann nicht mehr entstehen.

### Punkte 1 + 2 – Freilaufdioden und 24-V-Versorgung (extern gelöst)

Auf der Platine unverändert gegenüber v07: `Q1`/`Q2` (IRL540), je 100 Ω Gate-Serien-R und
10 kΩ Gate-Pulldown, Drain auf `SP1` bzw. `SP2`. Ein 24-V-Netz gibt es weiterhin nicht.

Laut Vorgabe sitzen die **Freilaufdioden direkt an den Spulen** und die **24-V-Versorgung auf
einer separaten Platine**. Damit ist der Punkt erledigt – mit zwei Auflagen für die Verdrahtung:

- Der **GND der 24-V-Platine muss mit dem Platinen-GND verbunden** sein, sonst hat der IRL540
  keinen Rückweg.
- ⚠ **`SP1`/`SP2` führen an *beiden* Polen dasselbe Netz** (`D_2` bzw. `D_3` = IRL540-Drain).
  Es sind **keine** Spulenklemmen mit +24 V und Drain, sondern ein doppelt herausgeführter
  Drain. Die +24-V-Seite der Spule gehört an die externe Platine.

### Zusätzlich neu in v1.0 (nicht angefragt)

- **GPIO3-Pulldown bestückt:** 10 kΩ von `HC595_SER_PIN` nach GND. Definiert gleichzeitig den
  Strapping-Zustand (JTAG-Quellwahl) und den `SER`-Eingang des 74HCT541. War in `Review_v07.md`
  nur als Verbesserung gelistet.

### Displaystecker J2 – korrigiert

Alle Signale sind in die **jeweils andere Pin-Reihe derselben Spalte** gewandert; die in v07
auf GND gelegten Gegenpins sind jetzt **`nc`**:

| Signal | v07-Pin | **v1.0-Pin** | Ziffer / Segment |
|---|:---:|:---:|---|
| `DIG_1` | 2 | **1** | Bip 1er |
| `DIG_5` | 4 | **3** | Bip 10er |
| `DIG_7` | 6 | **5** | Credit 1er |
| `DIG_3` | 8 | **7** | Credit 10er |
| `DIG_2` | 10 | **9** | Score 1er |
| `DIG_6` | 12 | **11** | Score 10er |
| `DIG_4` | 14 | **13** | Score 100er |
| `DIG_0` | 16 | **15** | Score 1000er |
| `SEG_DP` | 19 | **20** | dP2 |
| `SEG_G` | 21 | **22** | g |
| `SEG_F` | 23 | **24** | f |
| `SEG_E` | 25 | **26** | e |
| `SEG_D` | 27 | **28** | d |
| `SEG_C` | 29 | **30** | c |
| `SEG_B` | 31 | **32** | b |
| `SEG_A` | 33 | **34** | a |

Alle übrigen 18 Pins: **`nc`** (in v07 waren 16 davon GND).

Die Zuordnung **`DIG_n` → Ziffer ist unverändert** – nur die Steckerpins haben sich
verschoben. Das Firmware-Mapping bleibt damit gültig.

---

## 📌 Umnummerierte Referenz-Designatoren

Beim Vergleich mit älteren Notizen, dem v07-iBOM oder `Review_v07.md` beachten:

| Baustein | v07 | **v1.0** |
|---|---|---|
| 74HC595 (Kaskade) | IC1, IC2, IC4, IC5 | **IC1–IC4** |
| ESP32-S3-WROOM-1 | IC6 | **IC5** |
| AMS1117-3.3 | IC7 | **IC6** |
| 74HCT541 | IC14 | **IC7** |
| MAX7221 | X5 | **X4** |
| MAX98357A | M2 | **M1** |
| MCP23S17 | X1 | X1 |
| CH340C | X3 | X3 |
| 2N7002 (595-`/OE`) | T33 | T33 |

Neu hinzugekommen: **R55**, **R56**, **R57**, **R58** (10 kΩ), **D4** (SS24).
Die DIP-Isolationsdioden heißen jetzt **D1–D3** (v07: D51–D53).

---

## 📦 Stücklisten-Abgleich v07 → v1.0

Der Vergleich der beiden iBOM-Exporte bestätigt die Schaltplan-Prüfung unabhängig:
**134 → 141 Bauteile**, und die Differenz besteht **ausschließlich** aus den erwarteten
Korrekturen. Es ist nichts still mitverändert worden.

| Neu bestückt | Funktion |
|---|---|
| **R55, R56, R57, R58** – 4× 10 kΩ (0402) | 2N7002-Gate-Pulldown (Punkt 3) · GPIO3-Pulldown · **R57/R58 = SPI3-CS-Pull-ups** (Punkt 8) |
| **D4** – SS24 (SMB) | USB-VBUS-Entkopplung (Punkt 4) |
| GEH1, GEH2 | Silkscreen-Logos (LISY_development, CC_Logo) – keine elektrischen Bauteile |

Alles Übrige ist mengengleich; die restlichen Unterschiede sind reine Umbenennungen
(siehe Designator-Tabelle oben). Bestätigt außerdem: **R46 = 12 kΩ** als RSET am MAX7221,
**3× 1N4148W** (D1–D3) als DIP-Entkopplung, **32× AO3400A** für die Lampen.

---

## 🟡 Offene Punkte

1. **Keine Masse-Rückleiter mehr im Display-Band.** Bewusste Entscheidung (die
   Verschachtelung war eine Annahme der alten Doku, keine Eigenschaft der
   Original-Verdrahtung). Konsequenz: Die Signaladern koppeln direkt aufeinander, die
   Ghosting-Neigung über die ~1 m ist höher. Gegenmaßnahmen bleiben moderates RSET (12 kΩ) und
   SPI-Takt ~1 MHz; Rückfallebene ist Variante A (MAX7221 ins Display-Gehäuse). Im Betrieb
   beobachten.
2. **5-V-Strombudget und Ampacity** – unverändert offen aus v07: je Lampen-Header nur **ein**
   +5-V-Pin (J7 versorgt ~10 Lampen), und J1-Pin4 trägt den gesamten Board-Strom über einen
   einzelnen 2,54-mm-Pin. Gegen den realen Lampenstrom prüfen.
3. **IRL540 real prüfen** – Spulenstrom messen und gegen die Transfer-Kennlinie bei
   V_GS = 5 V gegenchecken.
4. **SW10–SW13 → GPA2–GPA5:** Die Reihenfolge innerhalb GPA2–GPA5 ist weiterhin nicht
   dokumentiert (die Steckerbelegung nennt für J4 keine Port-Zuordnung). Beim Test ermitteln
   und **zuerst hier** eintragen, dann im Firmware-Repo (`main/hwmap.h`).

---

## Aktionsliste (kompakt)

| # | Thema | Status |
|---|-------|--------|
| 1 | Spulen-Freilaufdioden | ✅ extern an den Spulen |
| 2 | 24-V-Versorgung | ✅ separate Platine (GND verbinden, SP-Klemmen beachten) |
| 3 | 2N7002-Gate-Pulldown | ✅ 10 kΩ am Gate → GND |
| 4 | USB-VBUS-Entkopplung | ✅ D4 (SS24) hinter F1 |
| 5 | Display-Segmentreihenfolge | ✅ Doku 8.1 folgt jetzt dem Schaltplan |
| 6 | MCP-Reserve GPA6/7 | ✅ Doku 9.2 aktualisiert |
| 7 | CH340C statt nativem USB | ✅ Doku 4/5 + `gpiodefs.h` aktualisiert |
| 8 | SPI3-`CS`-Pull-ups | ✅ R57/R58 je 10 kΩ → 3,3 V |
| — | GPIO3-Pulldown | ✅ bestückt (war nur Empfehlung) |
| — | J2-Belegung | ✅ korrigiert, Doku 8.1 neu |
| — | Ghosting, 5-V-Ampacity, IRL540, SW10–13 | 🟡 offen, siehe oben |
