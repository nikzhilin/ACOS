#include <stdio.h>

#include "line.h"
#include "stats.h"

/// всё по программам; "ожидание" = время на мойке минус время самих операций
static int washed[MAX_PROGRAMS];
static long timeSum[MAX_PROGRAMS];
static long waitSum[MAX_PROGRAMS];
static int timeMax = 0;
static int blockedTimes = 0;

void StatsListener(const Event *e, const struct Line *line) {
    if (e->type == EV_BLOCKED) {
        ++blockedTimes;
    }
    if (e->type != EV_DONE) {
        return;
    }
    const Car *car = &line->cars[e->car];
    int total = car->finished - car->arrived;
    washed[car->program]++;
    timeSum[car->program] += total;
    waitSum[car->program] += total - car->service;
    if (total > timeMax) {
        timeMax = total;
    }
}

static const char *Outcome(LineStatus status) {
    switch (status) {
    case LINE_FINISHED: return "finished: entrance closed and every accepted car washed";
    case LINE_STALLED: return "STALLED: remaining cars can never be served";
    case LINE_INTERRUPTED: return "INTERRUPTED by user";
    default: return "running";
    }
}

void StatsPrint(int fd, const struct Line *l) {
    const Config *cfg = l->cfg;
    char from[16], to[16];
    EventClock(0, from, sizeof(from));
    EventClock(l->now, to, sizeof(to));
    long allTime = 0, allWait = 0;
    for (int i = 0; i < cfg->programs; ++i) {
        allTime += timeSum[i];
        allWait += waitSum[i];
    }
    int served = l->served;
    double hours = l->now / 60.0;

    dprintf(fd, "\n=================== RESULTS ===================\n");
    dprintf(fd, "Outcome        : %s\n", Outcome(l->status));
    dprintf(fd, "Model time     : %s - %s (%d min)\n", from, to, l->now);
    dprintf(fd, "Cars arrived   : %d (accepted %d, turned away %d)\n", l->carCount,
            l->carCount - l->rejected, l->rejected);
    dprintf(fd, "Cars washed    : %d, still inside: %d\n", served, l->inside);
    if (served > 0) {
        dprintf(fd, "Time at wash   : avg %.1f min, max %d min\n", (double)allTime / served,
                timeMax);
        dprintf(fd, "Waiting        : avg %.1f min per car (queues, blocked posts, repairs)\n",
                (double)allWait / served);
    }
    if (hours > 0) {
        dprintf(fd, "Throughput     : %.1f cars per hour\n", served / hours);
    }
    dprintf(fd, "Failures       : %d, blocked-post episodes: %d\n", l->failures, blockedTimes);
    for (int i = 0; i < cfg->programs; ++i) {
        dprintf(fd, "%s%-9s %3d washed", i == 0 ? "By program     : " : "                 ",
                cfg->program[i].name, washed[i]);
        if (washed[i] > 0) {
            dprintf(fd, ", avg %.1f min", (double)timeSum[i] / washed[i]);
        }
        dprintf(fd, "\n");
    }
}
