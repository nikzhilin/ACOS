#include <stdio.h>

#include "view_log.h"

static int screen = -1;
static int useColor = 0;
static int journal = -1;

void ViewLogInit(int screenFd, int color, int journalFd) {
    screen = screenFd;
    useColor = color;
    journal = journalFd;
}

void ViewLogListener(const Event *e, const struct Line *line) {
    char text[200], clock[16];
    if (EventDescribe(e, line, text, sizeof(text)) == 0) {
        return;
    }
    EventClock(e->time, clock, sizeof(clock));
    if (screen >= 0) {
        if (useColor) {
            dprintf(screen, "\033[2m[%s]\033[0m %s%s\033[0m\n", clock, EventColor(e->type), text);
        } else {
            dprintf(screen, "[%s] %s\n", clock, text);
        }
    }
    if (journal >= 0) {
        dprintf(journal, "[%s] %s\n", clock, text);
    }
}
