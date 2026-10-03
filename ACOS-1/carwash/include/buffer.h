#ifndef BUFFER_H
#define BUFFER_H

// Накопитель перед стадией: очередь номеров машин с ограниченной вместимостью

#include "config.h"

typedef struct {
    int car[MAX_QUEUE];
    int count;
    int cap;
} Buffer;

// чем больше оценка, тем раньше машину заберут из очереди
typedef int (*ScoreFn)(int car, const void *ctx);

void BufferInit(Buffer *b, int cap);
void BufferPush(Buffer *b, int car);

// extra - сколько можно положить сверх cap (нужно только для режима -x)
int BufferHasRoom(const Buffer *b, int extra);

// достаёт машину с максимальной оценкой, при равенстве - ту, что пришла раньше;
// -1 если очередь пуста
int BufferPopBest(Buffer *b, ScoreFn score, const void *ctx);

#endif
