# Firmware – Kegelautomat (ESP-IDF)

Aktueller Stand: **Sound-Testprojekt** (`kegel_sound_test`) zum Erproben der
LEDC-PWM-Tonausgabe auf dem Steckbrett. Die Tonerzeugung steckt im wiederverwendbaren
Component [`components/sound/`](components/sound/README.md).

Die Demo (`main/main.c`) initialisiert den Sound und spielt in einer Endlosschleife
abwechselnd einen kurzen Piep (1,2 kHz) und ein abklingendes Plong (600 Hz).
Sound-Ausgang: **GPIO20** → Piezo bzw. RC + kleiner Verstärker → Lautsprecher
(siehe `Steuerplatine_Doku.md`, Abschnitt 13).

## Bauen (Windows, ESP-IDF v5.5.x)

> **Wichtig – zwei Windows-Fallstricke** (kosten sonst Stunden):
> 1. Das Projekt liegt auf `N:`, einem **gemappten Netzlaufwerk (UNC `\\server\…`)**.
>    CMD.EXE kann keinen UNC-Pfad als Arbeitsverzeichnis nutzen → der Build **muss in
>    ein lokales Build-Verzeichnis** gelenkt werden (`idf.py -B <lokal>`).
> 2. **MAX_PATH (260 Zeichen):** die tief verschachtelten ESP-IDF-Objektpfade sprengen
>    das Limit, wenn der Build-Pfad lang ist → **kurzes** Build-Verzeichnis wählen.

In der **ESP-IDF-PowerShell** (oder nach `. $HOME\esp\v5.5.1\esp-idf\export.ps1`):

```powershell
cd N:\Projekte\Kegelautomat\firmware
idf.py -B C:\Users\bonta\kb set-target esp32c3   # nur beim ersten Mal / Target-Wechsel
idf.py -B C:\Users\bonta\kb build
```

`C:\Users\bonta\kb` ist ein kurzer, lokaler Pfad – nicht im Repo, dient als Build-Cache.

## Flashen & Loggen

```powershell
idf.py -B C:\Users\bonta\kb -p COMx flash monitor
```

Die Konsole/Logs laufen über **USB-Serial/JTAG** (USB-C-Port, GPIO18/19), damit UART0
(GPIO20/21) frei bleibt – GPIO20 ist der Sound-Ausgang (siehe `sdkconfig.defaults`).
`monitor` mit `Strg+]` beenden.

## Struktur

```
firmware/
├─ CMakeLists.txt          project(kegel_sound_test)
├─ sdkconfig.defaults      Target esp32c3 + Konsole USB-Serial/JTAG
├─ main/                   Demo (app_main)
└─ components/sound/       LEDC-PWM-Tonausgabe (sound_init/beep/plong/stop)
```
