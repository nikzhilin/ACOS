#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "auditor.h"
#include "line.h"
#include "view_live.h"

#define RECENT     6   // сколько последних событий показывать внизу
#define CELL       7   // ширина полоски поста
#define DAY_WIDTH  30  // ширина полоски рабочего дня
#define TEXT_WIDTH 70

#define RESET  "\033[0m"
#define DIM    "\033[2m"
#define BOLD   "\033[1m"
#define RED    "\033[1;31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define EOL    "\033[K\n"  // стереть хвост старой строки

// номер машины красим цветом её программы
static const char *PROGRAM_COLOR[] = {"\033[1;36m", "\033[1;33m", "\033[1;35m",
                                      "\033[1;34m", "\033[1;32m", "\033[1;37m"};

static int out = 1;

typedef struct {
    EventType type;
    char clock[16];
    char text[TEXT_WIDTH];
} Recent;

static Recent recent[RECENT];
static int recentCount = 0, recentHead = 0;

/// кадр собираем в памяти и выводим одним write(), иначе экран мерцает
static char frame[32768];
static int frameLen = 0;

void ViewLiveInit(int fd) {
    out = fd;
    dprintf(out, "\033[2J\033[?25l");  // очистить экран, спрятать курсор
}

void ViewLiveEnd(void) {
    dprintf(out, "\033[?25h");
}

static void Put(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(frame + frameLen, sizeof(frame) - frameLen, fmt, args);
    va_end(args);
    if (n > 0) {
        frameLen += n;
        if (frameLen >= (int)sizeof(frame)) {
            frameLen = sizeof(frame) - 1;
        }
    }
}

static void Repeat(const char *glyph, int count) {
    for (int i = 0; i < count; ++i) {
        Put("%s", glyph);
    }
}

static void PutCar(const Line *l, int c, int width) {
    Put("%s%0*d" RESET, PROGRAM_COLOR[l->cars[c].program], width, c + 1);
}

/// Пустое место под номер машины: " ·" или "  ·"
static void PutEmpty(int width) {
    Put("%*s" DIM "·" RESET, width - 1, "");
}

/// Пост: номер машины и полоска, которая заполняется по ходу мойки.
/// Словами пишем только особые случаи: машина ждёт или пост в ремонте.
static void PutPost(const Line *l, const Post *post, int width) {
    if (post->car >= 0) {
        PutCar(l, post->car, width);
    } else {
        Put("%*s", width, "");
    }
    Put(" ");
    switch (PostStateOf(post)) {
    case POST_IDLE: Put(DIM "free   " RESET); break;
    case POST_BLOCKED: Put(YELLOW "waiting" RESET); break;
    case POST_BROKEN: Put(RED "fix %2dm" RESET, post->repair); break;
    case POST_BUSY: {
        int done = CELL * (post->duration - post->left) / post->duration;
        Put(GREEN);
        Repeat("█", done);
        Put(RESET DIM);
        Repeat("░", CELL - done);
        Put(RESET);
        break;
    }
    }
    Put("   ");
}

/// Очередь: машины и свободные места, выровнено по самой длинной очереди
static void PutQueue(const Line *l, int s, int width, int columnWidth) {
    const Buffer *q = &l->stage[s].queue;
    int slots = q->cap > q->count ? q->cap : q->count;
    for (int i = 0; i < slots; ++i) {
        if (i < q->count) {
            PutCar(l, q->car[i], width);
        } else {
            PutEmpty(width);
        }
        Put(" ");
    }
    Put("%*s", columnWidth - (slots * (width + 1)), "");
}

/// Шапка: время, полоска рабочего дня, что сейчас с линией и цвета программ
static void PutHeader(const Line *l) {
    char clock[16];
    EventClock(l->now, clock, sizeof(clock));
    Put(BOLD " CAR WASH " RESET " %s  ", clock);

    if (l->cfg->day > 0) {
        int done = l->now >= l->cfg->day ? DAY_WIDTH : DAY_WIDTH * l->now / l->cfg->day;
        Repeat("━", done);
        Put(DIM);
        Repeat("─", DAY_WIDTH - done);
        Put(RESET);
    } else {
        Put(DIM "no closing time" RESET);
    }

    switch (l->status) {
    case LINE_STALLED: Put(RED "  stalled" RESET); break;
    case LINE_FINISHED: Put(GREEN "  all cars washed" RESET); break;
    case LINE_INTERRUPTED: Put(YELLOW "  interrupted" RESET); break;
    default: Put(l->open ? GREEN "  entrance open" RESET : YELLOW "  entrance closed" RESET);
    }
    Put(EOL);

    Put(" ");
    for (int i = 0; i < l->cfg->programs; ++i) {
        Put("%s●" RESET DIM " %s   " RESET, PROGRAM_COLOR[i], l->cfg->program[i].name);
    }
    Put(EOL EOL);
}

/// Счётчики, аудитор и последние события
static void PutFooter(const Line *l) {
    Put(" washed %d" DIM " · " RESET "inside %d" DIM " · " RESET "turned away %d" DIM " · " RESET
        "breakdowns %d   ",
        l->served, l->inside, l->rejected, l->failures);
    if (AuditorViolations() == 0) {
        Put(GREEN "✓" RESET DIM " %ld checks ok" RESET EOL, AuditorChecks());
    } else {
        Put(RED "✗ %ld violations" RESET EOL, AuditorViolations());
    }
    Put(EOL);
    for (int i = 0; i < recentCount; ++i) {
        const Recent *r = &recent[(recentHead - recentCount + i + RECENT) % RECENT];
        Put(DIM " %s " RESET "%s%s" RESET EOL, r->clock, EventColor(r->type), r->text);
    }
}

static void Redraw(const Line *l) {
    const Config *cfg = l->cfg;
    int width = l->carCount >= 99 ? 3 : 2;  // сколько цифр в номере машины
    int columnWidth = 0;
    for (int s = 0; s < cfg->stages; ++s) {
        int w = cfg->stage[s].queue * (width + 1);
        columnWidth = w > columnWidth ? w : columnWidth;
    }
    columnWidth += 2;

    frameLen = 0;
    Put("\033[H");
    PutHeader(l);
    for (int s = 0; s < cfg->stages; ++s) {
        Put(" %-9s ", cfg->stage[s].name);
        PutQueue(l, s, width, columnWidth);
        if (cfg->stage[s].posts == 0) {
            Put(RED "no posts" RESET);
        }
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            PutPost(l, &l->stage[s].post[p], width);
        }
        Put(EOL);
    }
    Put(EOL);
    PutFooter(l);
    Put("\033[J");  // стереть всё, что ниже
    if (write(out, frame, (size_t)frameLen) < 0) {
        return;  // терминал закрыли - просто пропускаем кадр
    }
}

static void Remember(const Event *e, const Line *l) {
    char text[200];
    if (EventDescribe(e, l, text, sizeof(text)) == 0) {
        return;
    }
    Recent *r = &recent[recentHead];
    r->type = e->type;
    EventClock(e->time, r->clock, sizeof(r->clock));
    snprintf(r->text, sizeof(r->text), "%.*s", (int)sizeof(r->text) - 1, text);
    recentHead = (recentHead + 1) % RECENT;
    if (recentCount < RECENT) {
        ++recentCount;
    }
}

void ViewLiveListener(const Event *e, const struct Line *line) {
    Remember(e, line);
    if (e->type == EV_TICK || e->type == EV_INTERRUPT) {
        Redraw(line);
    }
}
