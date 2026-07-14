// sound.c – LEDC-PWM-Tonausgabe fuer den Kegelautomaten. Siehe sound.h.
//
// Prinzip: Die LEDC-Timer-Frequenz ist die Tonhoehe (Rechteck-Ton), Duty ~50 %.
//   beep()  -> Ton an, nach der Dauer wieder aus.
//   plong() -> Ton anschlagen und per Hardware-Duty-Fade (50 % -> 0) ausklingen
//              lassen; ein kleineres Duty ist (nach RC/Verstaerker) leiser.
// Bewusst ohne Timer-ISR gehalten – rein High-Level-LEDC-API.

#include "sound.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SOUND_GPIO          20                     // reservierter Sound-Pin (UART0-RX, sonst frei)
#define SOUND_LEDC_MODE     LEDC_LOW_SPEED_MODE     // ESP32-C3 kennt nur Low-Speed-Mode
#define SOUND_LEDC_TIMER    LEDC_TIMER_0
#define SOUND_LEDC_CHANNEL  LEDC_CHANNEL_0
#define SOUND_LEDC_RES      LEDC_TIMER_10_BIT       // 10 Bit Duty (0..1023)
#define SOUND_DUTY_50       (1u << (SOUND_LEDC_RES - 1))   // 512 @ 10 Bit ~ 50 %
#define SOUND_IDLE_FREQ_HZ  1000                    // Startfrequenz des Timers

static const char *TAG = "sound";

esp_err_t sound_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = SOUND_LEDC_MODE,
        .timer_num       = SOUND_LEDC_TIMER,
        .duty_resolution = SOUND_LEDC_RES,
        .freq_hz         = SOUND_IDLE_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config: %s", esp_err_to_name(err));
        return err;
    }

    ledc_channel_config_t channel = {
        .speed_mode = SOUND_LEDC_MODE,
        .channel    = SOUND_LEDC_CHANNEL,
        .timer_sel  = SOUND_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = SOUND_GPIO,
        .duty       = 0,          // still starten
        .hpoint     = 0,
    };
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config: %s", esp_err_to_name(err));
        return err;
    }

    // Fade-Service fuer sound_plong()
    err = ledc_fade_func_install(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_fade_func_install: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Sound bereit auf GPIO%d", SOUND_GPIO);
    return ESP_OK;
}

void sound_stop(void)
{
    ledc_set_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL, 0);
    ledc_update_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL);
}

void sound_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    ledc_set_freq(SOUND_LEDC_MODE, SOUND_LEDC_TIMER, freq_hz);
    ledc_set_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL, SOUND_DUTY_50);
    ledc_update_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    sound_stop();
}

void sound_plong(uint32_t freq_hz, uint32_t duration_ms)
{
    ledc_set_freq(SOUND_LEDC_MODE, SOUND_LEDC_TIMER, freq_hz);

    // Ton mit voller Lautstaerke anschlagen ...
    ledc_set_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL, SOUND_DUTY_50);
    ledc_update_duty(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL);

    // ... und ueber die Dauer auf 0 ausklingen lassen (nicht blockierend).
    ledc_set_fade_time_and_start(SOUND_LEDC_MODE, SOUND_LEDC_CHANNEL, 0,
                                 duration_ms, LEDC_FADE_NO_WAIT);
}
