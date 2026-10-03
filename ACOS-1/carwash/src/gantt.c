#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gantt.h"
#include "line.h"
#include "memory.h"

#define WIDTH     48  // с подписями строка как раз влезает в 80 колонок
#define LABEL_GAP 10

// в том же порядке, что и PostState
static const char SYMBOL[] = {'.', '#', '=', 'X'};
static const char *COLOR[] = {"\033[2m", "\033[32m", "\033[33m", "\033[1;31m"};

// timeline[s][p][t] - состояние поста p стадии s в минуту t
static char *timeline[MAX_STAGES][MAX_POSTS];
static int length = 0, capacity = 0;

void GanttListener(const Event *e, const struct Line *l) {
    if (e->type != EV_TICK) {
        return;
    }
    if (length == capacity) {
        capacity = capacity ? capacity * 2 : 256;
        for (int s = 0; s < MAX_STAGES; ++s) {
            for (int p = 0; p < MAX_POSTS; ++p) {
                timeline[s][p] = CheckedRealloc(timeline[s][p], (size_t)capacity);
            }
        }
    }
    for (int s = 0; s < l->cfg->stages; ++s) {
        for (int p = 0; p < l->cfg->stage[s].posts; ++p) {
            timeline[s][p][length] = (char)PostStateOf(&l->stage[s].post[p]);
        }
    }
    ++length;
}

// Шаг меток времени: "круглое" число минут, чтобы метки не слипались
static int LabelStep(int bucket) {
    static const int steps[] = {10, 15, 20, 30, 60, 120, 180, 240, 360, 720, 1440};
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) {
        if (steps[i] / bucket >= LABEL_GAP) {
            return steps[i];
        }
    }
    int step = 1440;
    while (step / bucket < LABEL_GAP) {
        step *= 2;  // прогон на много суток
    }
    return step;
}

// Что чаще всего было с постом на отрезке [from, to). Ремонт рисуем, даже если
// он занял только треть отрезка, иначе короткие поломки не видно.
static int Dominant(const char *row, int from, int to) {
    int count[4] = {0};
    for (int t = from; t < to; ++t) {
        count[(int)row[t]]++;
    }
    if (count[POST_BROKEN] * 3 >= to - from) {
        return POST_BROKEN;
    }
    int best = POST_BUSY;
    if (count[POST_BLOCKED] > count[best]) {
        best = POST_BLOCKED;
    }
    if (count[POST_IDLE] > count[best]) {
        best = POST_IDLE;
    }
    return best;
}

void GanttPrint(int fd, const struct Line *l, int color) {
    const Config *cfg = l->cfg;
    if (length == 0) {
        return;
    }
    int bucket = (length + WIDTH - 1) / WIDTH;  // минут в одной колонке
    int cols = (length + bucket - 1) / bucket;

    dprintf(fd, "\nTIMELINE OF POSTS, 1 column = %d min:  # busy  = blocked  X broken  . idle\n",
            bucket);
    int step = LabelStep(bucket);
    char axis[WIDTH + LABEL_GAP + 1];
    memset(axis, ' ', sizeof(axis));
    for (int minute = 0; minute / bucket < cols; minute += step) {
        char clock[16];
        int len = EventClock(minute, clock, sizeof(clock));
        int col = minute / bucket;
        if (col + len <= cols) {
            memcpy(axis + col, clock, (size_t)len);
        }
    }
    axis[cols] = '\0';
    dprintf(fd, "%13s%-*s  busy block repair\n", "", cols, axis);

    double bestBusy = -1, bestBlocked = 0;
    int busiest = -1, mostBlocked = -1;
    for (int s = 0; s < cfg->stages; ++s) {
        long stageBusy = 0, stageBlocked = 0;
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            const char *row = timeline[s][p];
            dprintf(fd, "%-8s P%d |", cfg->stage[s].name, p + 1);
            for (int c = 0; c < cols; ++c) {
                int to = (c + 1) * bucket < length ? (c + 1) * bucket : length;
                int state = Dominant(row, c * bucket, to);
                if (color) {
                    dprintf(fd, "%s%c\033[0m", COLOR[state], SYMBOL[state]);
                } else {
                    dprintf(fd, "%c", SYMBOL[state]);
                }
            }
            long count[4] = {0};
            for (int t = 0; t < length; ++t) {
                count[(int)row[t]]++;
            }
            dprintf(fd, "| %3ld%%  %3ld%%  %3ld%%\n", 100 * count[POST_BUSY] / length,
                    100 * count[POST_BLOCKED] / length, 100 * count[POST_BROKEN] / length);
            stageBusy += count[POST_BUSY];
            stageBlocked += count[POST_BLOCKED];
        }
        if (cfg->stage[s].posts == 0) {
            dprintf(fd, "%-8s    -- no posts --\n", cfg->stage[s].name);
            continue;
        }
        double busy = (double)stageBusy / (cfg->stage[s].posts * length);
        double blocked = (double)stageBlocked / (cfg->stage[s].posts * length);
        if (busy > bestBusy) {
            bestBusy = busy;
            busiest = s;
        }
        if (blocked > bestBlocked) {
            bestBlocked = blocked;
            mostBlocked = s;
        }
    }
    if (busiest >= 0) {
        dprintf(fd, "Bottleneck     : %s (posts busy %.0f%% of the time)\n",
                cfg->stage[busiest].name, 100 * bestBusy);
    }
    if (mostBlocked >= 0) {
        dprintf(fd, "Most blocked   : %s (posts held finished cars %.0f%% of the time)\n",
                cfg->stage[mostBlocked].name, 100 * bestBlocked);
    }
}

void GanttFree(void) {
    for (int s = 0; s < MAX_STAGES; ++s) {
        for (int p = 0; p < MAX_POSTS; ++p) {
            free(timeline[s][p]);
            timeline[s][p] = NULL;
        }
    }
    length = capacity = 0;
}
