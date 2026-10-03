#ifndef LINE_H
#define LINE_H

// Координатор линии: принимает машины, двигает их по стадиям,
// закрывает въезд и решает, когда моделирование закончено.

#include "buffer.h"
#include "car.h"
#include "config.h"
#include "post.h"

typedef enum {
    LINE_RUNNING,
    LINE_FINISHED,
    LINE_STALLED,
    LINE_INTERRUPTED,
} LineStatus;

typedef struct {
    Post post[MAX_POSTS];
    Buffer queue;
} Stage;

typedef struct Line {
    const Config *cfg;
    Stage stage[MAX_STAGES];

    Car *cars;  // номер машины = индекс в массиве
    int carCount, carCap;

    int now;  // текущая минута
    int nextArrival;
    int open;    // въезд открыт
    int inside;  // принятые и ещё не уехавшие
    int moved;   // кто-то сдвинулся за эту минуту (для поиска тупика)
    LineStatus status;

    int served, rejected, failures;
} Line;

void LineInit(Line *l, const Config *cfg);
void LineTick(Line *l);
void LineInterrupt(Line *l);
void LineFree(Line *l);

#endif
