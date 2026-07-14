# Component `sound` – Soundeffekte via LEDC-PWM

Einfache Effekte („piep" / „plong") für den Kegelautomaten, direkt vom ESP32-C3
per **LEDC-PWM** erzeugt. Kein DAC-IC, **kein Eingriff in den SPI-Bus oder den
74HCT541** der Steuerplatine → keine Rückwirkung auf Lampen/Displays/Kontakte.

## API

```c
esp_err_t sound_init(void);                            // einmal beim Start
void      sound_beep (uint32_t freq_hz, uint32_t ms);  // Ton an, nach ms aus (blockiert)
void      sound_plong(uint32_t freq_hz, uint32_t ms);  // Ton mit Abkling-Fade (nicht blockierend)
void      sound_stop (void);                           // Stille
```

## Funktionsprinzip

- Die LEDC-Timer-Frequenz ist die **Tonhöhe** (Rechteck-Ton), Duty ≈ 50 %.
- `beep` schaltet den Ton für eine feste Dauer ein/aus.
- `plong` schlägt den Ton an und lässt ihn per **LEDC-Duty-Fade** (50 % → 0) über
  die Dauer ausklingen (kleineres Duty ⇒ nach RC/Verstärker leiser).
- Konfiguration oben in `sound.c` (`SOUND_GPIO`, Auflösung, Idle-Frequenz).

## Verdrahtung

Sound-Pin: **GPIO20**.

- **Schnelltest:** passiver Piezo-Buzzer GPIO20 → GND (ggf. 100–330 Ω in Reihe).
- **Mit Lautsprecher:**
  `GPIO20 → RC-Tiefpass (R ≈ 1 kΩ, C ≈ 100 nF) → kleiner Mono-Verstärker (5 V) → Lautsprecher (8 Ω)`.
- Die Konsole/Logs laufen über **USB-Serial/JTAG** (GPIO18/19, siehe
  `sdkconfig.defaults`), damit UART0/GPIO20 frei für den Sound bleibt.

## Möglicher Ausbau (später)

- Echte Sinus-Hüllkurve per Sample-Streaming (gptimer @ Fs, Duty = Wellenform) für
  weicheren Klang.
- Höhere Klangqualität über einen externen **MCP4921-DAC** am gemeinsamen SPI-Bus
  (/CS = GPIO20) – aktuell **geparkt**, erst nach dem LEDC-Test.
