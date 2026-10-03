#ifndef EVENTS_H
#define EVENTS_H

// Шина событий. line.c ничего не печатает, а только сообщает, что произошло.
// Лог, живая схема, аудитор, Гант и статистика подписываются на события.

struct Line;

typedef enum {
    EV_ARRIVE,
    EV_PROGRAM,  // arg = номер программы
    EV_REJECT,
    EV_QUEUE,  // arg = сколько машин теперь в очереди
    EV_ENTER,
    EV_OP_START,  // arg = длительность
    EV_OP_END,
    EV_BLOCKED,  // arg = стадия, в которую не пускают
    EV_LEAVE,
    EV_MOVE,  // stage = куда, arg = откуда
    EV_FAIL,  // arg = время ремонта
    EV_REPAIR,
    EV_DONE,  // arg = сколько минут машина провела на мойке
    EV_CLOSE,
    EV_STALL,  // stage = стадия без постов, из-за которой всё встало (или -1)
    EV_INTERRUPT,
    EV_TICK,  // конец минуты
} EventType;

typedef struct {
    EventType type;
    int time;
    int car, stage, post;  // -1, если не относится к событию
    int arg;
} Event;

typedef void (*Listener)(const Event *e, const struct Line *line);

void EventsSubscribe(Listener listener);
void EventsEmit(const struct Line *line, EventType type, int car, int stage, int post, int arg);

// текст события для лога; 0, если у события нет текста (EV_TICK)
int EventDescribe(const Event *e, const struct Line *line, char *buf, int size);

// ANSI-цвет строки события: поломки красные, ожидание жёлтое и т.д.
const char *EventColor(EventType type);

// "08:17", а со вторых суток "d2 08:17"
int EventClock(int minute, char *buf, int size);

#endif
