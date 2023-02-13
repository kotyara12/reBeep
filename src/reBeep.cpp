#include "reBeep.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" 
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "driver/ledc.h"
#include "rLog.h"
#include "def_beep.h"
#include "def_tasks.h"

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

#define BEEP_QUEUE_ITEM_SIZE sizeof(beep_data_t)

static QueueHandle_t _beepQueue = NULL;
static TaskHandle_t _beepTask = NULL;
static uint8_t _pinBuzzer = 0;
static const char* beepTaskName = "buzzer";

#if CONFIG_BEEP_STATIC_ALLOCATION
static StaticQueue_t _beepQueueBuffer;
static StaticTask_t _beepTaskBuffer;
static StackType_t _beepTaskStack[CONFIG_BEEP_TASK_STACK_SIZE];
static uint8_t _beepQueueStorage[CONFIG_BEEP_QUEUE_SIZE * BEEP_QUEUE_ITEM_SIZE];
#endif // CONFIG_BEEP_STATIC_ALLOCATION

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
    if (xQueueReceive(_beepQueue, &data, portMAX_DELAY) == pdPASS) {
      xLastWakeTime = xTaskGetTickCount();
      for (uint8_t i = 0; i < data.count; i++) {
        // Turn on the sound
        esp_task_wdt_reset();
        ledc_set_freq(ledc_channel.speed_mode, ledc_channel.timer_sel, data.frequency);
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, data.duration);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // We are waiting for the specified time
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(data.duration));
        // Turn off the sound
        esp_task_wdt_reset();
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // If this is not the last sound in the series, then we are waiting again
        if (i < data.count - 1) {
          esp_task_wdt_reset();
          vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(data.duration));
        };
      };
    };
  };

  beepTaskDelete();
}

void beepTaskCreate(const uint8_t pinBuzzer)
{
  _pinBuzzer = pinBuzzer;
  
  if (!_beepQueue) {
    #if CONFIG_BEEP_STATIC_ALLOCATION
    _beepQueue = xQueueCreateStatic(CONFIG_BEEP_QUEUE_SIZE, BEEP_QUEUE_ITEM_SIZE, &(_beepQueueStorage[0]), &_beepQueueBuffer);
    #else
    _beepQueue = xQueueCreate(CONFIG_BEEP_QUEUE_SIZE, BEEP_QUEUE_ITEM_SIZE);
    #endif // CONFIG_BEEP_STATIC_ALLOCATION
    if (!_beepQueue) {
      rloga_e("Failed to create a queue for buzzer!");
      return;
    }
  };
  
  if (!_beepTask) {
    #if CONFIG_BEEP_STATIC_ALLOCATION
    _beepTask = xTaskCreateStaticPinnedToCore(beepTaskExec, beepTaskName, 
      CONFIG_BEEP_TASK_STACK_SIZE, nullptr, CONFIG_TASK_PRIORITY_BEEP, _beepTaskStack, &_beepTaskBuffer, CONFIG_TASK_CORE_BEEP); 
    #else
    xTaskCreatePinnedToCore(beepTaskExec, beepTaskName, 
      CONFIG_BEEP_TASK_STACK_SIZE, nullptr, CONFIG_TASK_PRIORITY_BEEP, &_beepTask, CONFIG_TASK_CORE_BEEP); 
    #endif // CONFIG_BEEP_STATIC_ALLOCATION
    if (!_beepTask) {
      rloga_e("Failed to create task for buzzer!");
      beepTaskDelete();
    } else {
      rloga_i("Task [ %s ] has been successfully created", beepTaskName);
    };
  };
}

void beepTaskDelete()
{
  if (_beepTask) {
    vTaskDelete( _beepTask);
     _beepTask = NULL;
  };

  if (_beepQueue) {
    vQueueDelete( _beepQueue);
     _beepQueue = NULL;
  };
}

bool beepTaskSend(const uint16_t frequency, const uint16_t duration, const uint8_t count)
{
  if (_beepQueue && (frequency > 0) && (duration > 0) && (count > 0)) {
    static beep_data_t data;
    data.frequency = frequency;
    data.duration = duration;
    data.count = count;

    if (xQueueSend( _beepQueue, &data, CONFIG_BEEP_QUEUE_WAIT) == pdPASS) {
      return true;
    };
  };
  return false;
}


