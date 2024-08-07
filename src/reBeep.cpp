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
#define CONFIG_BEEP_DRES     LEDC_TIMER_13_BIT     // resolution of PWM duty
#define CONFIG_BEEP_DUTY     4096                  // duty
#define CONFIG_BEEP_FREQ     3000                  // frequency of PWM signal 
#define CONFIG_BEEP_MODE     LEDC_HIGH_SPEED_MODE  // timer mode

typedef struct {
  uint16_t frequency1;
  uint16_t frequency2;
  uint16_t duration;
  uint8_t  count;
  uint16_t duty;
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
  // Инициализация LEDC
  {
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));
    ledc_timer.duty_resolution = CONFIG_BEEP_DRES;
    ledc_timer.freq_hz = CONFIG_BEEP_FREQ;           
    ledc_timer.speed_mode = CONFIG_BEEP_MODE;
    ledc_timer.timer_num = CONFIG_BEEP_TIMER;
    ledc_timer_config(&ledc_timer);
  };

  ledc_channel_config_t ledc_channel;
  memset(&ledc_channel, 0, sizeof(ledc_channel));
  ledc_channel.channel = CONFIG_BEEP_CHANNEL;
  ledc_channel.duty = 0;
  ledc_channel.gpio_num = _pinBuzzer;
  ledc_channel.speed_mode = CONFIG_BEEP_MODE;
  ledc_channel.timer_sel = CONFIG_BEEP_TIMER;
  ledc_channel_config(&ledc_channel);

  ledc_fade_func_install(0);
  
  static beep_data_t data;
  static TickType_t xLastWakeTime;
  while (1) {
    if (xQueueReceive(_beepQueue, &data, portMAX_DELAY) == pdPASS) {
      xLastWakeTime = xTaskGetTickCount();
      for (uint8_t i = data.count; i > 0; i--) {
        // Воспроизводим частоту 1
        ledc_set_freq(ledc_channel.speed_mode, ledc_channel.timer_sel, data.frequency1);
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, data.duty);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(data.duration));

        // Воспроизводим частоту 2 или отключаем звук
        if (data.frequency2 > 0) {
          ledc_set_freq(ledc_channel.speed_mode, ledc_channel.timer_sel, data.frequency2);
          ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, data.duty);
        } else {
          ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
        };
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(data.duration));
      };
      // Серия закончена, отключаем звук
      ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
      ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
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

bool beepTaskSend(const uint16_t frequency1, const uint16_t frequency2, const uint16_t duration, const uint8_t count, const uint16_t duty)
{
  if (_beepQueue && (frequency1 > 0) && (duration > 0) && (duty > 0) && (count > 0)) {
    beep_data_t data;
    data.frequency1 = frequency1;
    data.frequency2 = frequency2;
    data.duration = duration;
    data.count = count;
    data.duty = duty;

    if (xQueueSend(_beepQueue, &data, CONFIG_BEEP_QUEUE_WAIT) == pdPASS) {
      return true;
    };
  };
  return false;
}


