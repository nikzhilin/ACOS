#include "buffer.h"

#include <string.h>

void BufferInit(Buffer *b, int cap) {
    b->count = 0;
    b->cap = cap;
}

int BufferHasRoom(const Buffer *b, int extra) {
    return b->count < b->cap + extra && b->count < MAX_QUEUE;
}

void BufferPush(Buffer *b, int car) {
    if (b->count < MAX_QUEUE) {
        b->car[b->count++] = car;
    }
}

int BufferPopBest(Buffer *b, ScoreFn score, const void *ctx) {
    if (b->count == 0) {
        return -1;
    }
    int best = 0;
    for (int i = 1; i < b->count; ++i) {
        if (score(b->car[i], ctx) > score(b->car[best], ctx)) {
            best = i;
        }
    }
    int car = b->car[best];
    memmove(&b->car[best], &b->car[best + 1], (size_t)(b->count - best - 1) * sizeof(int));
    --b->count;
    return car;
}
