/**
 * @file line.h
 * @brief Координатор линии: принимает машины, двигает их по стадиям,
 * закрывает въезд и решает, когда моделирование закончено.
 */
#ifndef LINE_H
#define LINE_H

#include "buffer.h"
#include "car.h"
#include "config.h"
#include "post.h"

typedef enum {
    LINE_RUNNING,
    LINE_FINISHED,     ///< въезд закрыт, все уехали
    LINE_STALLED,      ///< тупик
    LINE_INTERRUPTED,  ///< SIGINT / SIGTERM
} LineStatus;

/// Стадия на линии: посты и очередь перед ними.
typedef struct {
    Post post[MAX_POSTS];
    Buffer queue;
} Stage;

typedef struct Line {
    const Config *cfg;
    Stage stage[MAX_STAGES];

    Car *cars;  ///< номер машины = индекс в массиве
    int carCount, carCap;

    int now;  ///< текущая минута
    int nextArrival;
    int open;    ///< въезд открыт
    int inside;  ///< принятые и ещё не уехавшие
    int moved;   ///< кто-то сдвинулся за эту минуту (для поиска тупика)
    LineStatus status;

    int served, rejected, failures;
} Line;

void LineInit(Line *l, const Config *cfg);

/**
 * @brief Одна минута модели.
 *
 * Поломки и ремонты, затем обход стадий от последней к первой,
 * прибытие новой машины и проверка, не пора ли заканчивать.
 */
void LineTick(Line *l);

/// Остановка по сигналу: статус LINE_INTERRUPTED и событие EV_INTERRUPT.
void LineInterrupt(Line *l);
void LineFree(Line *l);

#endif
