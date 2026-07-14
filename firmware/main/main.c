// Kegelautomat – Sound-Test
//
// Demo: initialisiert die LEDC-PWM-Tonausgabe und spielt in einer Endlosschleife
// abwechselnd einen kurzen Piep und ein abklingendes Plong. Dient zum Erproben der
// Sound-Firmware auf dem Steckbrett (GPIO20 -> Piezo bzw. RC + Verstaerker).

#include "sound.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(sound_init());
    ESP_LOGI(TAG, "Sound-Test gestartet (GPIO20): Piep + Plong im Wechsel.");

    while (true) {
        ESP_LOGI(TAG, "beep  1200 Hz");
        sound_beep(1200, 150);
        vTaskDelay(pdMS_TO_TICKS(700));

        ESP_LOGI(TAG, "plong  600 Hz");
        sound_plong(600, 300);
        vTaskDelay(pdMS_TO_TICKS(1000));   // Ausklingen + Pause
    }
}
