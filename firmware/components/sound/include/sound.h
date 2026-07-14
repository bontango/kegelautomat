// sound.h – Einfache Soundeffekte fuer den Kegelautomaten (ESP32-C3, LEDC-PWM)
//
// Erzeugt ein "piep"/"plong" direkt per LEDC-PWM auf einem einzelnen GPIO
// (SOUND_GPIO, Default GPIO20). Der SPI-Bus und der 74HCT541 der Steuerplatine
// bleiben unberuehrt -> keine Rueckwirkung auf Lampen/Displays/Kontakte.

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// LEDC-Timer/Kanal + Fade-Service auf dem Sound-GPIO einrichten. Einmal beim Start.
esp_err_t sound_init(void);

// Kurzer Piepton. freq_hz = Tonhoehe (z. B. 1000..2000), duration_ms = Dauer.
// Blockiert fuer duration_ms.
void sound_beep(uint32_t freq_hz, uint32_t duration_ms);

// "Plong": Ton anschlagen und ueber duration_ms per LEDC-Duty-Fade ausklingen lassen.
// Kehrt sofort zurueck; das Abklingen laeuft in Hardware weiter.
void sound_plong(uint32_t freq_hz, uint32_t duration_ms);

// Sofort verstummen (Duty 0).
void sound_stop(void);

#ifdef __cplusplus
}
#endif
