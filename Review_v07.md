# Review Prototyp‑Platine v07

> **Historisch – abgelöst durch [`Review_v10.md`](Review_v10.md).** Die Punkte 1–8 sind in
> Revision v1.0 erledigt. Dieses Dokument bleibt unverändert als Nachweis, was am
> Prototyp v07 gefunden wurde.

**Geprüft am:** 2026-07-17
**Ergänzt am:** 2026-07-28 – Punkt **8** aus der Inbetriebnahme der bestückten Platine
(SPI3-`CS` ohne definierten Boot-Zustand); Punkte 5–7 sind in der Doku erledigt.
**Grundlage:** `datasheets/Kegelautomat_v07_SCH.PDF`, `_PCB.PDF`, `iBOM_Kegelautomat_v07.html`
**Abgeglichen gegen:** `Steuerplatine_Doku.md`, `gpiodefs.h`, `datasheets/Display.jpg`
**BOM:** 134 Bauteile, 31 Gruppen.

Verifikation des v07‑Entwurfs gegen die vorhandene Dokumentation. Prüfmethode:
Schaltplan‑Netliste (Pin‑/Netz‑Ebene) rekonstruiert und gegen `gpiodefs.h`, die
Doku‑Tabellen und das Original‑Display (`Display.jpg`) abgeglichen; PCB nur visuell
aus dem Raster‑PDF (keine echte DRC/Netz‑für‑Netz‑Prüfung möglich).

---

## Kurzfazit

Elektrisch weitgehend sauber und deckungsgleich mit der Doku. **Vor dem Fertigen**
sollten die Punkte **1, 3, 4** und **8** angegangen werden. Punkt **5** ist kein
Platinen‑, sondern ein Doku‑Fehler (der Schaltplan ist dort korrekt). Punkt **8**
stammt nicht aus der Entwurfsprüfung, sondern aus der Inbetriebnahme der bestückten
Platine – er ist am realen Aufbau reproduzierbar aufgetreten.

---

## ✅ Verifiziert & korrekt

- **ESP32‑S3‑Portbelegung: 1:1 identisch mit `gpiodefs.h`** — alle 22 belegten GPIOs
  (SD, I²S, SPI3, 595, Spulen, Taster, DIP). Keine Verletzung der DO‑NOT‑USE‑Liste
  (33–37 PSRAM frei, 26–32 Flash frei, Strapping beachtet).
- **74HCT541 (IC14):** Kanalbelegung exakt nach Doku §6 (I1/I2=Spulen→Y1/Y2,
  I3–I5=595, I6–I8=MAX). `/OE1`+`/OE2`→GND, VCC=5 V, C9 abgeblockt.
- **74HC595‑Kaskade:** IC1→IC2→IC4→IC5 (SEROUT korrekt durchgereicht), 32 Ausgänge
  auf Header (30 + 2 Reserve), `/SRCLR`=5 V, je 100 Ω Gate‑R + 10‑kΩ‑Array‑Pulldown
  (SIL9) pro AO3400.
- **MAX7221 (X5):** RSET=**12 kΩ**, Serien‑R 100 Ω an CLK/DIN/CS, **beide GND‑Pins
  (4+9)**, 22 µF+100 nF Abblockung, DOUT offen, Pinout korrekt
  (Pin20=SEGC / 21=SEGE / 23=SEGD stimmen mit Datenblatt).
- **MCP23S17 (X1):** VDD=**3,3 V**, A0/A1/A2→GND, INTA→IO6 / **INTB offen**,
  `/RESET`→10 kΩ→3,3 V, C13 abgeblockt.
- **DIP‑Multiplex (S4 + D51/D52/D53):** Die drei 1N4148W sind die
  **Isolationsdioden der DIP‑Schalter** auf den I²S‑Leitungen — ersetzt den in
  Doku §13.3 beschriebenen Tri‑State‑Puffer und ist **inhärent 3,3‑V‑sicher**
  (nur ESP‑Pins beteiligt). Löst den offenen Punkt §12/7.
- **J2‑Masse‑Verschachtelung** (Signal–GND–Signal–GND) beibehalten.
- **EN‑Reset** (S3 + C5 100 nF + Pull‑up), **SD‑Karte** @3,3 V mit Pull‑ups.
- **Abblockung:** je 595/541/MAX/MCP/CH340 ein 100 nF; 22 µF‑Bulks an Eingang,
  MAX7221 und AMS1117‑Ausgang.

---

## 🔴 Fehler / zu beheben (priorisiert)

### 1. Spulen‑Freilaufdioden fehlen komplett — höchste Priorität
An Q1/Q2 (IRL540) ist **nur die intrinsische Body‑Diode** vorhanden — die schützt
gegen Induktionsspannung **nicht** (falsche Richtung). Doku §9.4/§12.2 fordert sie
ausdrücklich („zwingend je Spule"). Ohne Freilaufdiode sieht der IRL540 beim
Abschalten der 24‑V‑Spule Spitzen ≫ 100 V (Vds‑max) → wiederholte Avalanche,
mittelfristig Ausfall.

**Fix:** Diode (UF4007/1N4007) direkt über jede Spule (Kathode an +24 V, Anode an
Drain) — extern am Automaten oder on‑board (siehe Punkt 2).

### 2. Keine 24‑V‑Versorgung / SP1/SP2 führen nur den Drain
Board‑Eingang J1 (PSW5) liefert **nur +5 V**; AMS1117 macht 3,3 V. **Ein 24‑V‑Zweig
existiert nicht.** SP1/SP2 haben **beide Pole am selben Netz = IRL540‑Drain**
(Netze `D_2` / `D_3`). Die Spulen‑Plusseite (+24 V) und damit auch die
Freilaufdiode können auf der Platine derzeit nicht angeschlossen werden.

**Fix / Klärung:** Kommt die 24 V extern am Automaten (dann muss GND gemeinsam
sein)? Sauberer: SP‑Klemme umbelegen (Pol1 = Drain, Pol2 = +24‑V‑Eingang) und die
Freilaufdiode on‑board setzen.

### 3. 2N7002 (T33): Gate‑Pulldown fehlt — widerspricht dem Boot‑Blanking‑Konzept
Das Gate hängt nur über 100 Ω an GPIO16; die in Doku §6.1 geforderte **10 kΩ
Gate→GND** fehlt. Beim Boot ist GPIO16 hochohmig → Gate floatet → 2N7002‑Zustand
undefiniert → `/OE` undefiniert. Mit dem zufälligen 595‑Latch‑Inhalt können Lampen
kurz aufblitzen. Der 10‑kΩ‑Drain‑Pull‑up (→ /OE=5 V = Lampen aus) wirkt nur, wenn
der FET sicher **aus** ist.

**Fix:** 10 kΩ von GPIO16‑Gate nach GND ergänzen. Billiger, wichtiger Fix.

### 4. USB‑VBUS ↔ +5 V ohne Entkopplung
VBUS ist über die PTC‑Sicherung F1 (MF‑MSMF150‑2) **direkt mit dem +5‑V‑Rail**
verbunden — keine OR‑Diode/kein Power‑MUX. Sind USB und externes 5‑V‑Netzteil
gleichzeitig gesteckt, speisen beide gegeneinander (Rückspeisung in den PC‑Port
über die PTC).

**Fix:** Schottky in Reihe zu VBUS (VBUS→F1→Schottky→+5 V) oder Constraint klar
dokumentieren („nie beide gleichzeitig"). Volllast (Lampen bis 3 A) ist ohnehin nur
über J1 möglich, F1 = 1,5 A.

### 8. SPI3‑`CS` (GPIO18/GPIO15): kein Pull‑up → undefiniert beim Boot
*Nachtrag 2026-07-28, an der bestückten Platine gefunden – nicht aus der Entwurfsprüfung.*

Beide Chip‑Selects hängen ohne Pull‑up am 74HCT541 (Kanal 3 = GPIO18 → MAX7221‑`CS`;
GPIO15 geht direkt an den MCP23S17). Bis `spibus_init()` sind die ESP‑Pins hochohmig,
der 541 macht aus dem floatenden Eingang einen beliebigen 5‑V‑Pegel. Liegt `CS` dabei
LOW, schiebt der MAX7221 Störflanken von `CLK`/`DIN` als Frame ein und übernimmt sie
mit der nächsten steigenden CS‑Flanke.

**Symptom am realen Aufbau:** Nach dem Einschalten leuchten alle 8 Ziffern kurz hell
als `8`, danach erscheint die reguläre `0`. Das ist die Signatur des
**Display‑Test‑Registers** — laut Datenblatt übersteuert es *„all controls and digit
registers (including the shutdown register)"* und bleibt aktiv, *„until the display‑test
register is reconfigured"*; der Blitz endet daher exakt mit dem ersten Kommando der
Firmware (`0x0F 0x00`). Der POR‑Zustand kann es nicht sein: dort ist die Anzeige
geblankt, im Shutdown, Scan‑Limit 1 Digit und Intensity auf **Minimum** — also weder
alle 8 Digits noch hell.

Systematisch ist das dieselbe Klasse wie Punkt 3: ein floatender Eingang, für den das
Boot‑Konzept (Doku §11) einen definierten Zustand vorsieht, der aber auf der Platine
nicht erzwungen wird. Beim MCP23S17 wiegt es schwerer als beim Display — ein
Zufallsframe könnte `IODIR` umstellen und Eingänge zu Ausgängen machen, die gegen die
Kontaktschalter treiben.

**Fix:** je **10 kΩ nach 3,3 V**, beide auf der **ESP‑Seite** — bei GPIO18 an den
541‑**Eingang** (am 541‑Ausgang wirkungslos, der treibt bei `/OE` = GND aktiv), bei
GPIO15 direkt an das Netz, das ohne Puffer zum MCP läuft. **Auf 3,3 V**, nie 5 V, weil
der S3 nicht 5‑V‑tolerant ist. `CLK`/`DIN` brauchen nichts: liegt `CS` sicher HIGH,
schiebt keiner der beiden Bausteine ein. Billiger Fix, gleiche Kategorie wie Punkt 3.

**Firmwareseitig bereits entschärft:** `spibus_park_cs()` parkt beide CS als allererstes
in `app_main()`, die Display‑Initialisierung läuft vor der SD‑Karte. Das schließt das
Fenster ab `app_main()` — die Bootloader‑Zeit davor bleibt ohne die Pull‑ups offen.

**Nachtest 2026‑07‑28:** Mit dieser Firmware tritt der Blitz nicht mehr sichtbar auf.
Der Punkt bleibt trotzdem offen: Ob im Restfenster ein Frame mit gesetztem
Display‑Test‑Bit zusammenkommt, ist Zufall, und wo ein floatender Eingang ruht, hängt
von Leckstrom, Temperatur und Verschmutzung ab. Erst die Pull‑ups machen aus „geht
gerade" ein „geht immer" — zwei Widerstände, gleiche Kategorie wie Punkt 3.

---

## 📝 Doku muss korrigiert werden (Schaltplan ist richtig)

### 5. Display‑Segmentreihenfolge — Doku §8.1 ist verkehrt herum
Die handgezeichnete `Display.jpg` zeigt **Pin 19=dP2, 21=g, 23=f, 25=e, 27=d,
29=c, 31=b, 33=a**. Der **Schaltplan setzt genau das um** (J2 Pin19=SEG_DP …
Pin33=SEG_A). Die Doku‑Tabelle §8.1 listet die Segmente **verkehrt** als
„a b c d e f g dP2".

**→ Schaltplan korrekt, Doku §8.1 anpassen.** Sonst droht Code‑B‑Fehlanzeige, wenn
später jemand nach der Doku verdrahtet.

### 6. MCP‑Reserve‑Pins verschoben
Doku §9.2: Kontakte auf GPA0‑7 + GPB0‑5, Reserve GPB6/7. Schaltplan: GPB0‑7 +
GPA0‑5, **Reserve = GPA6/GPA7** (beide `nc`, nicht auf Header geführt). Funktional
14 + 2, aber Doku aktualisieren — und überlegen, die 2 Reserve‑Eingänge auf ein
Pad/Header zu legen.

### 7. Programmier‑Interface weicht von Doku/`gpiodefs.h` ab
Statt nativem USB (IO19/20) nutzt die Platine **CH340C an UART0 (IO43/44)** +
UMH3N‑Auto‑Reset; **IO19/20 sind `nc`**. Solide Wahl (robuster Auto‑Download),
aber: 43/44 sind damit **nicht** mehr „frei/Debug", dafür 19/20 frei. Doku §4/§5 und
den `gpiodefs.h`‑Kommentar entsprechend nachziehen.

---

## 💡 Verbesserungen / zu prüfen

- **GPIO3 (595‑SER) Strapping‑Pulldown** (Doku §12.8) — konnte ich nicht finden;
  10 kΩ→GND ergänzen (definierter JTAG‑Strap + SER‑Eingang).
- **Digit‑Scan‑Mapping dokumentieren:** J2 verteilt DIG0‑7 bewusst „verwürfelt" auf
  Bip/Credit/Score (Pin2=DIG_1, Pin4=DIG_5, Pin6=DIG_7, Pin8=DIG_3, Pin10=DIG_2,
  Pin12=DIG_6, Pin14=DIG_4, Pin16=DIG_0). Kein Fehler, aber gehört als
  Firmware‑Mapping‑Tabelle in die Doku.
- **+5‑V‑Verteilung:** je Lampen‑Header nur **ein** +5‑V‑Pin; J7 versorgt ~10
  Lampen (bis ~1–2 A über einen 2,54‑mm‑Pin). Auch J1‑Pin4 (+5 V, einzeln) trägt
  den gesamten Board‑Strom. Ampacity von Pin + Bahn gegen den realen Lampenstrom
  prüfen; ggf. +5 V doppeln.
- **MAX98357A `SD_MODE`/`GAIN`** (M2) — Beschaltung prüfen, damit der Amp nicht im
  Shutdown hängt bzw. (L+R)/2 gewählt ist. Spannungstabelle (`SD_MODE`) + Gain-Stufen
  jetzt in Doku §13.1.
- **AMS1117‑3.3** ~0,85 W bei 0,5 A — thermisch ok, kritisch nur bei intensivem WLAN.
- **100 nF direkt am ESP‑3V3‑Pin** ergänzen (Modul hat interne Cs, dennoch empfohlen).
- **ESP‑Footprint** in der Lib heißt `ESP32S3WROOM1N8R2` — Modul‑Outline/Pinout ist
  über alle WROOM‑1‑Varianten identisch, der Wert (N16R8) stimmt → **pin‑kompatibel,
  kein Problem**, nur ein Namensartefakt.

---

## Routing (visuell aus dem Raster‑PDF, keine DRC)

Positiv: Power‑/GND‑Bahnen im Lampenbereich deutlich verbreitert; 541 ↔ MAX7221 kurz
beieinander (Variante B); MCP kurz am ESP; J2‑GND‑Verschachtelung erhalten. Eine
echte Netz‑für‑Netz‑/DRC‑Prüfung geht nur im CAD — aus dem PDF lassen sich nur
Bahnbreiten/Anordnung beurteilen, nicht jede einzelne Verbindung.

---

## Aktionsliste (kompakt)

| # | Thema | Typ | Aktion |
|---|-------|-----|--------|
| 1 | Spulen‑Freilaufdioden | HW‑Fehler | Diode je Spule (UF4007/1N4007) |
| 2 | 24‑V‑Versorgung / SP‑Klemmen | HW‑Klärung | 24‑V‑Konzept + SP‑Belegung festlegen |
| 3 | 2N7002‑Gate‑Pulldown | HW‑Fehler | 10 kΩ GPIO16‑Gate → GND |
| 4 | USB‑VBUS ↔ +5 V | HW‑Verbesserung | Schottky an VBUS oder Constraint |
| 5 | Display‑Segmentreihenfolge | Doku‑Fehler | Doku §8.1 an Schaltplan/Display.jpg |
| 6 | MCP‑Reserve GPA6/7 | Doku‑Abgleich | Doku §9.2 aktualisieren |
| 7 | CH340 statt nativem USB | Doku‑Abgleich | Doku §4/§5 + gpiodefs.h |
| 8 | SPI3‑`CS` ohne Pull‑up | HW‑Fehler | 10 kΩ → 3,3 V an 541‑Eingang GPIO18 **und** an Netz GPIO15 |
| — | GPIO3‑Pulldown, Digit‑Map, +5‑V‑Ampacity, SD_MODE, ESP‑3V3‑C | Prüfen | siehe oben |
