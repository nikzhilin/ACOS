#include <stdio.h>

#include "events.h"
#include "line.h"

#define MAX_LISTENERS 8
#define DAY_START     (8 * 60)  // мойка открывается в 08:00

static Listener listeners[MAX_LISTENERS];
static int listenerCount = 0;

void EventsSubscribe(Listener listener) {
    if (listenerCount < MAX_LISTENERS) {
        listeners[listenerCount++] = listener;
    }
}

void EventsEmit(const struct Line *line, EventType type, int car, int stage, int post, int arg) {
    Event e = {type, line->now, car, stage, post, arg};
    for (int i = 0; i < listenerCount; ++i) {
        listeners[i](&e, line);
    }
}

const char *EventColor(EventType type) {
    switch (type) {
    case EV_FAIL: return "\033[1;31m";
    case EV_REPAIR: return "\033[1;32m";
    case EV_DONE: return "\033[32m";
    case EV_REJECT:
    case EV_BLOCKED: return "\033[33m";
    case EV_ARRIVE:
    case EV_PROGRAM: return "\033[36m";
    case EV_CLOSE:
    case EV_STALL:
    case EV_INTERRUPT: return "\033[1;35m";
    default: return "\033[0m";
    }
}

int EventClock(int minute, char *buf, int size) {
    int t = DAY_START + minute;
    int day = (t / (24 * 60)) + 1;
    if (day == 1) {
        return snprintf(buf, size, "%02d:%02d", t / 60, t % 60);
    }
    return snprintf(buf, size, "d%d %02d:%02d", day, (t / 60) % 24, t % 60);
}

int EventDescribe(const Event *e, const Line *l, char *buf, int size) {
    const Config *cfg = l->cfg;
    const char *st = e->stage >= 0 ? cfg->stage[e->stage].name : "";
    const char *prog = e->car >= 0 ? cfg->program[l->cars[e->car].program].name : "";
    int id = e->car + 1;  // людям привычнее нумерация с 1
    int post = e->post + 1;

    switch (e->type) {
    case EV_ARRIVE: return snprintf(buf, size, "car #%d arrives at the entrance", id);
    case EV_PROGRAM: {
        // "Premium (prewash > tunnel > wax > dry)"
        const ProgramSpec *p = &cfg->program[e->arg];
        int n = snprintf(buf, size, "car #%d chooses %s (", id, p->name);
        for (int k = 0; k < p->len && n < size; ++k) {
            n += snprintf(buf + n, size - n, "%s%s", k ? " > " : "", cfg->stage[p->route[k]].name);
        }
        if (n < size) {
            n += snprintf(buf + n, size - n, ")");
        }
        return n;
    }
    case EV_REJECT:
        return cfg->stage[e->stage].queue == 0
                   ? snprintf(buf, size,
                              "car #%d turned away: no free %s post and no waiting space", id, st)
                   : snprintf(buf, size, "car #%d turned away: %s queue is full (%d/%d)", id, st,
                              e->arg, cfg->stage[e->stage].queue);
    case EV_QUEUE:
        return snprintf(buf, size, "car #%d waits in %s queue (%d/%d)", id, st, e->arg,
                        cfg->stage[e->stage].queue);
    case EV_ENTER: return snprintf(buf, size, "car #%d enters %s post %d", id, st, post);
    case EV_OP_START:
        return snprintf(buf, size, "%s post %d starts on car #%d %s (%d min)", st, post, id, prog,
                        e->arg);
    case EV_OP_END: return snprintf(buf, size, "%s post %d finished car #%d", st, post, id);
    case EV_BLOCKED:
        return snprintf(buf, size, "car #%d waits on %s post %d: %s is full", id, st, post,
                        cfg->stage[e->arg].name);
    case EV_LEAVE: return snprintf(buf, size, "car #%d leaves %s post %d", id, st, post);
    case EV_MOVE:
        return snprintf(buf, size, "car #%d moves %s -> %s", id, cfg->stage[e->arg].name, st);
    case EV_FAIL:
        return e->car >= 0 ? snprintf(buf, size,
                                      "%s post %d BREAKS DOWN with car #%d on it (repair %d min)",
                                      st, post, id, e->arg)
                           : snprintf(buf, size, "%s post %d BREAKS DOWN (repair %d min)", st, post,
                                      e->arg);
    case EV_REPAIR:
        return snprintf(buf, size, "%s post %d is repaired and back in service", st, post);
    case EV_DONE:
        return snprintf(buf, size, "car #%d is clean! %s done in %d min, drives away", id, prog,
                        e->arg);
    case EV_CLOSE:
        return snprintf(buf, size, "ENTRANCE CLOSED, %d accepted car(s) still inside", e->arg);
    case EV_STALL:
        return e->stage >= 0
                   ? snprintf(buf, size,
                              "STALL: %d car(s) can never be served, stage %s has no posts", e->arg,
                              st)
                   : snprintf(buf, size, "STALL: %d car(s) can never be served", e->arg);
    case EV_INTERRUPT:
        return snprintf(buf, size, "INTERRUPTED by user, %d car(s) left inside", e->arg);
    case EV_TICK:
    default: return 0;
    }
}
