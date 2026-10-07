/**
 * @file buffer.h
 * @brief Накопитель перед стадией: очередь номеров машин с ограниченной вместимостью.
 */
#ifndef BUFFER_H
#define BUFFER_H

#include "config.h"

/// Очередь машин перед стадией.
typedef struct {
    int car[MAX_QUEUE];  ///< номера машин в порядке прихода
    int count;           ///< сколько машин сейчас в очереди
    int cap;             ///< вместимость из конфига
} Buffer;

/// Оценка машины для правила приоритета: чем больше, тем раньше её заберут.
typedef int (*ScoreFn)(int car, const void *ctx);

void BufferInit(Buffer *b, int cap);
void BufferPush(Buffer *b, int car);

/**
 * @brief Есть ли место ещё для одной машины.
 * @param extra сколько можно положить сверх cap (нужно только для режима -x)
 */
int BufferHasRoom(const Buffer *b, int extra);

/**
 * @brief Достаёт машину с максимальной оценкой, при равенстве - ту, что пришла раньше.
 * @return номер машины или -1, если очередь пуста
 */
int BufferPopBest(Buffer *b, ScoreFn score, const void *ctx);

#endif
