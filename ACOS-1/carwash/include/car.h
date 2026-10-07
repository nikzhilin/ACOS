/**
 * @file car.h
 * @brief Автомобиль: выбранная программа, текущая стадия маршрута и место на линии.
 */
#ifndef CAR_H
#define CAR_H

#include "config.h"

/// Где сейчас машина.
typedef enum {
    CAR_QUEUE,
    CAR_POST,
    CAR_DONE,
    CAR_REJECTED,  ///< не пустили: на въезде не было места
} CarPlace;

typedef struct {
    int program;
    int step;  ///< индекс текущей стадии в маршруте программы
    CarPlace place;
    int stage;
    int post;     ///< -1, пока машина в очереди
    int blocked;  ///< помыта, но дальше некуда ехать
    int arrived, finished;
    int service;  ///< сколько минут её реально мыли
} Car;

/// Приехала новая машина: программу выбираем случайно с учётом весов.
void CarInit(Car *car, const Config *cfg, int now);

/// Стадия, на которой машина сейчас по маршруту.
int CarStage(const Car *car, const Config *cfg);

/// Следующая стадия маршрута или -1, если маршрут кончился.
int CarNextStage(const Car *car, const Config *cfg);

/// Сколько примерно минут мойки осталось (по средним временам стадий), для правила shortest.
int CarRemainingWork(const Car *car, const Config *cfg);

#endif
