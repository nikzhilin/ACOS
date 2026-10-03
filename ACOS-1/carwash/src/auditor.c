#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auditor.h"
#include "line.h"
#include "memory.h"

#define INVARIANTS  5
#define MAX_REPORTS 10  // больше сообщений всё равно никто читать не будет

static int errOut = 2;
static long checks = 0;
static long violations = 0;
static int reports = 0;
static long perInvariant[INVARIANTS + 1];

// Свой учёт маршрутов, собранный только из событий
static int *stagesSeen = NULL;  // сколько стадий маршрута машина уже начала
static int *stageNow = NULL;    // на какой стадии она сейчас, -1 - нигде
static int *places = NULL;      // в скольких местах нашли машину при осмотре
static int capacity = 0;
static int firstActive = 0;  // все машины до этого номера уже уехали

// тоже только по событиям FAIL / REPAIR
static int broken[MAX_STAGES][MAX_POSTS];

static const char *invariantName[INVARIANTS + 1] = {
    "",
    "a post serves at most one car",
    "a car is in exactly one place",
    "stages follow the program order",
    "a broken post starts no operation",
    "queue capacity is not exceeded",
};

void AuditorInit(int errFd) {
    errOut = errFd;
}

static void Grow(int cars) {
    if (cars <= capacity) {
        return;
    }
    int newCap = capacity ? capacity : 64;
    while (newCap < cars) {
        newCap *= 2;
    }
    stagesSeen = CheckedRealloc(stagesSeen, (size_t)newCap * sizeof(int));
    stageNow = CheckedRealloc(stageNow, (size_t)newCap * sizeof(int));
    places = CheckedRealloc(places, (size_t)newCap * sizeof(int));
    for (int i = capacity; i < newCap; ++i) {
        stagesSeen[i] = 0;
        stageNow[i] = -1;
    }
    capacity = newCap;
}

// Пока ошибка не исправлена, она находится после каждого события.
// Считаем все случаи, а печатаем каждое сообщение только один раз.
static void Violation(int inv, const Event *e, const char *fmt, ...) {
    static char printed[MAX_REPORTS][200];
    char text[200], clock[16];
    ++violations;
    ++perInvariant[inv];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    for (int i = 0; i < reports; ++i) {
        if (strcmp(text, printed[i]) == 0) {
            return;
        }
    }
    if (reports == MAX_REPORTS) {
        return;
    }
    snprintf(printed[reports++], sizeof(printed[0]), "%s", text);
    EventClock(e->time, clock, sizeof(clock));
    dprintf(errOut, isatty(errOut) ? "\033[1;41m AUDITOR \033[0m" : "AUDITOR:");
    dprintf(errOut, " [%s] I%d violated (%s): %s\n", clock, inv, invariantName[inv], text);
}

// I1, I2, I5: обходим все очереди и посты
static void CheckPlaces(const Event *e, const Line *l) {
    const Config *cfg = l->cfg;
    // уехавшие машины не перебираем, иначе на долгом прогоне будет O(n^2)
    while (firstActive < l->carCount &&
           (l->cars[firstActive].place == CAR_DONE || l->cars[firstActive].place == CAR_REJECTED)) {
        ++firstActive;
    }
    memset(places + firstActive, 0, (size_t)(l->carCount - firstActive) * sizeof(int));

    for (int s = 0; s < cfg->stages; ++s) {
        const Stage *st = &l->stage[s];
        if (st->queue.count > cfg->stage[s].queue) {
            Violation(5, e, "%s queue holds %d cars, capacity is %d", cfg->stage[s].name,
                      st->queue.count, cfg->stage[s].queue);
        }
        for (int i = 0; i < st->queue.count; ++i) {
            int c = st->queue.car[i];
            ++places[c];
            if (c < firstActive) {
                Violation(2, e, "car #%d has left but is still in %s queue", c + 1,
                          cfg->stage[s].name);
            }
        }
        for (int p = 0; p < MAX_POSTS; ++p) {
            int c = st->post[p].car;
            if (c < 0) {
                continue;
            }
            ++places[c];
            if (c < firstActive) {
                Violation(2, e, "car #%d has left but is still on %s post %d", c + 1,
                          cfg->stage[s].name, p + 1);
            }
            if (p >= cfg->stage[s].posts) {
                Violation(1, e, "non-existent %s post %d holds car #%d", cfg->stage[s].name, p + 1,
                          c + 1);
            }
        }
    }
    for (int c = firstActive; c < l->carCount; ++c) {
        const Car *car = &l->cars[c];
        int inside = car->place == CAR_QUEUE || car->place == CAR_POST;
        if (places[c] != inside) {
            Violation(2, e, "car #%d is found in %d places", c + 1, places[c]);
        }
        if (car->place == CAR_POST && l->stage[car->stage].post[car->post].car != c) {
            Violation(1, e, "%s post %d is claimed by car #%d but holds car #%d",
                      cfg->stage[car->stage].name, car->post + 1, c + 1,
                      l->stage[car->stage].post[car->post].car + 1);
        }
    }
}

// I3: новая стадия должна быть следующей по маршруту
static void CheckRoute(const Event *e, const Line *l) {
    int c = e->car;
    const ProgramSpec *p = &l->cfg->program[l->cars[c].program];
    if (e->type == EV_DONE) {
        if (stagesSeen[c] != p->len) {
            Violation(3, e, "car #%d left after %d of %d stages", c + 1, stagesSeen[c], p->len);
        }
        stageNow[c] = -1;
        return;
    }
    if (e->stage == stageNow[c]) {
        return;  // из очереди на пост той же стадии
    }
    if (stagesSeen[c] >= p->len || p->route[stagesSeen[c]] != e->stage) {
        Violation(3, e, "car #%d entered %s out of %s order", c + 1, l->cfg->stage[e->stage].name,
                  p->name);
    }
    ++stagesSeen[c];
    stageNow[c] = e->stage;
}

void AuditorListener(const Event *e, const struct Line *line) {
    Grow(line->carCount);
    ++checks;

    switch (e->type) {
    case EV_FAIL: broken[e->stage][e->post] = 1; break;
    case EV_REPAIR: broken[e->stage][e->post] = 0; break;
    case EV_OP_START:
        if (broken[e->stage][e->post]) {
            Violation(4, e, "%s post %d started car #%d while broken",
                      line->cfg->stage[e->stage].name, e->post + 1, e->car + 1);
        }
        break;
    case EV_QUEUE:
    case EV_ENTER:
    case EV_DONE: CheckRoute(e, line); break;
    default: break;
    }
    CheckPlaces(e, line);
}

long AuditorChecks(void) {
    return checks;
}
long AuditorViolations(void) {
    return violations;
}

void AuditorPrint(int fd) {
    dprintf(fd, "AUDITOR: invariants checked after each of %ld events, violations: %ld\n", checks,
            violations);
    for (int i = 1; i <= INVARIANTS; ++i) {
        dprintf(fd, "  I%d %-36s %s", i, invariantName[i], perInvariant[i] ? "FAILED" : "ok");
        if (perInvariant[i]) {
            dprintf(fd, " (%ld times)", perInvariant[i]);
        }
        dprintf(fd, "\n");
    }
}

void AuditorFree(void) {
    free(stagesSeen);
    free(stageNow);
    free(places);
    stagesSeen = stageNow = places = NULL;
    capacity = firstActive = 0;
}
