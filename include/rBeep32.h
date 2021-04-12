/* 
 *  EN: Library for playing simple sound signals
 *  RU: Библиотека воспроизведения простых звуковых сигналов
 *  ------------------------------------------------------------------------------------------------
 *  (с) 2020-2021 Разживин Александр | Razzhivin Alexander
 *  https://kotyara12.ru | kotyara12@yandex.ru | tg: @kotyara1971
 *
*/

#ifndef _RBEEP32_H_
#define _RBEEP32_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void beepTaskCreate(const uint8_t pinBuzzer);
void beepTaskDelete();
bool beepTaskPlay(const uint16_t frequency, const uint16_t duration, const uint8_t count);

#ifdef __cplusplus
}
#endif

#endif // _RBEEP32_H_