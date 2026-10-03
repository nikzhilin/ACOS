#ifndef CAR_H
#define CAR_H

#include "config.h"

typedef enum {
    CAR_QUEUE,
    CAR_POST,
    CAR_DONE,
    CAR_REJECTED,  // не пустили: на въезде не было места
} CarPlace;

typedef struct {
    int program;
    int step;  // индекс текущей стадии в маршруте программы
    CarPlace place;
    int stage, post;  // post = -1, пока машина в очереди
    int blocked;      // помыта, но дальше некуда ехать
    int arrived, finished;
    int service;  // сколько минут её реально мыли
} Car;

// приехала новая машина: программу выбираем случайно с учётом весов
void CarInit(Car *car, const Config *cfg, int now);

int CarStage(const Car *car, const Config *cfg);
int CarNextStage(const Car *car, const Config *cfg);  // -1, если маршрут кончился
int CarRemainingWork(const Car *car, const Config *cfg);

#endif
