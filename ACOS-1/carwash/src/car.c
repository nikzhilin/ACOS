#include "car.h"
#include "rng.h"

void CarInit(Car *car, const Config *cfg, int now) {
    int total = 0;
    for (int i = 0; i < cfg->programs; ++i) {
        total += cfg->program[i].weight;
    }
    // бросаем число от 1 до суммы весов и смотрим, в чей отрезок попали
    int roll = RngRange(1, total);
    int p = 0;
    while (roll > cfg->program[p].weight) {
        roll -= cfg->program[p++].weight;
    }
    car->program = p;
    car->step = 0;
    car->place = CAR_QUEUE;
    car->stage = cfg->program[p].route[0];
    car->post = -1;
    car->blocked = 0;
    car->arrived = now;
    car->finished = -1;
    car->service = 0;
}

int CarStage(const Car *car, const Config *cfg) {
    return cfg->program[car->program].route[car->step];
}

int CarNextStage(const Car *car, const Config *cfg) {
    const ProgramSpec *p = &cfg->program[car->program];
    return car->step + 1 < p->len ? p->route[car->step + 1] : -1;
}

int CarRemainingWork(const Car *car, const Config *cfg) {
    const ProgramSpec *p = &cfg->program[car->program];
    int work = 0;
    for (int k = car->step; k < p->len; ++k) {
        const StageSpec *s = &cfg->stage[p->route[k]];
        work += (s->tmin + s->tmax) / 2;
    }
    return work;
}
