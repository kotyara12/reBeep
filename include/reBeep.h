/* 
 *  EN: Library for playing simple sound signals
 *  RU: Библиотека воспроизведения простых звуковых сигналов
 *  ------------------------------------------------------------------------------------------------
 *  (с) 2020-2024 Разживин Александр | Razzhivin Alexander
 *  https://kotyara12.ru | kotyara12@yandex.ru | tg: @kotyara1971
 *
*/

#ifndef __RE_BEEP_H__
#define __RE_BEEP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Создание и запуск задачи по воспроизведению звуков на пищалке
 * */
void beepTaskCreate(const uint8_t pinBuzzer);

/**
 * @brief Удаление задачи по воспроизведению звуков на пищалке
 * */
void beepTaskDelete();

/**
 * @brief Неблокирующее (асинхронное) воспроизведение комбинации звуков
 * @param frequency1 Основная частота в Гц, должна быть больше 0
 * @param frequency2 Дополнительная частота в Гц или 0 для прерывистого звучания
 * @param duration Длительность двучания одной ноты в мс
 * @param count Количество звуков в серии от 1 до 255
 * @param duty Мощность (громкость) от 1 до 4096
 * @return Успех или неуспех
 * */
bool beepTaskSend(const uint16_t frequency1, const uint16_t frequency2, const uint16_t duration, const uint8_t count, const uint16_t duty);

#ifdef __cplusplus
}
#endif

#endif // __RE_BEEP_H__