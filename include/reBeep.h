/* 
 *  EN: Library for playing simple sound signals
 *  RU: Библиотека воспроизведения простых звуковых сигналов
 *  ------------------------------------------------------------------------------------------------
 *  (с) 2020-2021 Разживин Александр | Razzhivin Alexander
 *  https://kotyara12.ru | kotyara12@yandex.ru | tg: @kotyara1971
 *
*/

#ifndef __RE_BEEP_H__
#define __RE_BEEP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void beepTaskCreate(const uint8_t pinBuzzer);
void beepTaskDelete();
bool beepTaskSend(const uint16_t frequency, const uint16_t duration, const uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // __RE_BEEP_H__