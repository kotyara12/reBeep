#include "reBeep.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" 
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "driver/ledc.h"

#define CONFIG_BEEP_QUEUE_SIZE 3
#define CONFIG_BEEP_QUEUE_WAIT 10
#define CONFIG_BEEP_TASK_STACK_SIZE 2048
#define CONFIG_BEEP_TASK_PRIORITY 3
#define CONFIG_BEEP_TASK_CORE 0

#define CONFIG_BEEP_TIMER    LEDC_TIMER_0          // timer index (0-3)
#define CONFIG_BEEP_CHANNEL  LEDC_CHANNEL_0        // channel index (0-7)
#define CONFIG_BEEP_DUTY     LEDC_TIMER_13_BIT     // resolution of PWM duty
#define CONFIG_BEEP_FREQ     3000                  // frequency of PWM signal 
#define CONFIG_BEEP_MODE     LEDC_HIGH_SPEED_MODE  // timer mode

typedef struct {
  uint16_t frequency;
  uint16_t duration;
  uint8_t count;
} beep_data_t;

QueueHandle_t _queueBeep = NULL;
TaskHandle_t _taskBeep = NULL;
uint8_t _pinBuzzer = 0;

void beepTaskExec(void *pvParameters)
{
  beep_data_t data;
  TickType_t xLastWakeTime;
  // Initialize ledc timer
  ledc_timer_config_t ledc_timer;
  memset(&ledc_timer, 0, sizeof(ledc_timer));
  ledc_timer.duty_resolution = CONFIG_BEEP_DUTY;
  ledc_timer.freq_hz = CONFIG_BEEP_FREQ;           
  ledc_timer.speed_mode = CONFIG_BEEP_MODE;
  ledc_timer.timer_num = CONFIG_BEEP_TIMER;
  ledc_timer_config(&ledc_timer);
  // Initialize ledc channel
  ledc_channel_config_t ledc_channel;
  memset(&ledc_channel, 0, sizeof(ledc_channel));
  ledc_channel.channel = CONFIG_BEEP_CHANNEL;
  ledc_channel.duty = 0;
  ledc_channel.gpio_num = _pinBuzzer;
  ledc_channel.speed_mode = CONFIG_BEEP_MODE;
  ledc_channel.timer_sel = CONFIG_BEEP_TIMER;
  ledc_channel_config(&ledc_channel);
  // Initialize fade service
  ledc_fade_func_install(0);
  
  while (1) {
    if (xQueueReceive(_queueBeep, &data, portMAX_DELAY) == pdPASS) {
      xLastWakeTime = xTaskGetTickCount();
      for (uint8_t i = 0; i < data.count; i++) {
        // Turn on the sound
        esp_task_wdt_reset();
        ledc_set_freq(ledc_channel.speed_mode, ledc_channel.timer_sel, data.frequency);
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, data.duration);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // We are waiting for the specified time
        vTaskDelayUntil(&xLastWakeTime, (data.duration / portTICK_RATE_MS));
        // Turn off the sound
        esp_task_wdt_reset();
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // If this is not the last sound in the series, then we are waiting again
        if (i < data.count - 1) {
          esp_task_wdt_reset();
          vTaskDelayUntil(&xLastWakeTime, (data.duration / portTICK_RATE_MS));
        };
      };
    };
  };

  beepTaskDelete();
}

void beepTaskCreate(const uint8_t pinBuzzer)
{
  _pinBuzzer = pinBuzzer;
  
  if (!_queueBeep) {
     _queueBeep = xQueueCreate(CONFIG_BEEP_QUEUE_SIZE, sizeof(beep_data_t));
    if (!_queueBeep) {
      return;
    }
  };
  
  if (!_taskBeep) {
    xTaskCreatePinnedToCore(beepTaskExec, "buzzerBeep", CONFIG_BEEP_TASK_STACK_SIZE, NULL, CONFIG_BEEP_TASK_PRIORITY, &_taskBeep, CONFIG_BEEP_TASK_CORE); 
    if (!_taskBeep) {
      beepTaskDelete();
    };
  };
}

void beepTaskDelete()
{
  if (_taskBeep) {
    vTaskDelete( _taskBeep);
     _taskBeep = NULL;
  };

  if (_queueBeep) {
    vQueueDelete( _queueBeep);
     _queueBeep = NULL;
  };
}

bool beepTaskSend(const uint16_t frequency, const uint16_t duration, const uint8_t count)
{
  if (!_queueBeep) {
    beep_data_t data;
    data.frequency = frequency;
    data.duration = duration;
    data.count = count;

    if (xQueueSend( _queueBeep, &data, CONFIG_BEEP_QUEUE_WAIT) == pdPASS) {
      return true;
    };
  };
  return false;
}


