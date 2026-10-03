// Координатор линии.
//
// Каждую минуту стадии обходим с конца (от сушки к въезду), как конвейер:
// машина уехала с сушки - в эту же минуту на её место встаёт машина из
// тоннеля, на место той - машина с предмойки и т.д. Поэтому блокировки
// снимаются сами, отдельно искать "кто кого ждёт" не нужно.
//
// Сначала меняем состояние, потом шлём события - чтобы аудитор видел
// линию уже в целом виде, а не посередине перестановки.

#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "line.h"
#include "memory.h"
#include "rng.h"

#define EMIT(type, car, stage, post, arg) EventsEmit(l, (type), (car), (stage), (post), (arg))

void LineInit(Line *l, const Config *cfg) {
    memset(l, 0, sizeof(*l));
    l->cfg = cfg;
    for (int s = 0; s < cfg->stages; ++s) {
        for (int p = 0; p < MAX_POSTS; ++p) {
            PostInit(&l->stage[s].post[p]);
        }
        BufferInit(&l->stage[s].queue, cfg->stage[s].queue);
    }
    l->open = 1;
    l->status = LINE_RUNNING;
    l->nextArrival = 0;  // первая машина уже ждёт открытия
}

void LineFree(Line *l) {
    free(l->cars);
    l->cars = NULL;
}

// Свободный пост, на который можно заехать сразу, мимо очереди.
// Если в очереди кто-то стоит, без очереди не пускаем: -1.
static int DirectPost(const Line *l, int s) {
    if (l->stage[s].queue.count > 0) {
        return -1;
    }
    for (int p = 0; p < l->cfg->stage[s].posts; ++p) {
        if (PostStateOf(&l->stage[s].post[p]) == POST_IDLE) {
            return p;
        }
    }
    return -1;
}

// Есть ли на стадии s место ещё для одной машины.
// С -x здесь специально ошибка на единицу - проверка, что аудитор её поймает.
static int CanAccept(const Line *l, int s) {
    return DirectPost(l, s) >= 0 || BufferHasRoom(&l->stage[s].queue, l->cfg->sabotage);
}

// Кого первым забрать из очереди (правило priority из конфига)
static int Score(int c, const void *ctx) {
    const Line *l = ctx;
    const Car *car = &l->cars[c];
    switch (l->cfg->rule) {
    case PRIO_PROGRAM: return l->cfg->program[car->program].priority;
    case PRIO_SHORTEST: return -CarRemainingWork(car, l->cfg);
    default: return 0;  // fifo: оценки равны, BufferPopBest возьмёт того, кто раньше
    }
}

static void StartOnPost(Line *l, int c, int s, int p) {
    const StageSpec *spec = &l->cfg->stage[s];
    int duration = RngRange(spec->tmin, spec->tmax);
    Car *car = &l->cars[c];
    PostStart(&l->stage[s].post[p], c, duration);
    car->place = CAR_POST;
    car->stage = s;
    car->post = p;
    car->service += duration;
    l->moved = 1;
}

// Ставит машину на стадию s: на свободный пост или в очередь.
// Место должно быть заранее проверено через CanAccept.
static void Place(Line *l, int c, int s) {
    int p = DirectPost(l, s);
    if (p >= 0) {
        StartOnPost(l, c, s, p);
        return;
    }
    BufferPush(&l->stage[s].queue, c);
    l->cars[c].place = CAR_QUEUE;
    l->cars[c].stage = s;
    l->cars[c].post = -1;
    l->moved = 1;
}

// События о том, куда в итоге попала машина после Place
static void Announce(Line *l, int c) {
    const Car *car = &l->cars[c];
    if (car->place == CAR_QUEUE) {
        EMIT(EV_QUEUE, c, car->stage, -1, l->stage[car->stage].queue.count);
    } else {
        EMIT(EV_ENTER, c, car->stage, car->post, 0);
        EMIT(EV_OP_START, c, car->stage, car->post, l->stage[car->stage].post[car->post].left);
    }
}

static void Close(Line *l) {
    l->open = 0;
    EMIT(EV_CLOSE, -1, -1, -1, l->inside);
}

static void UpdateEquipment(Line *l) {
    const Config *cfg = l->cfg;
    for (int s = 0; s < cfg->stages; ++s) {
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            Post *post = &l->stage[s].post[p];
            if (PostRepairStep(post)) {
                EMIT(EV_REPAIR, post->car, s, p, 0);
            } else {
                int repair = PostTryBreak(post, cfg->failP, cfg->repairMin, cfg->repairMax);
                if (repair > 0) {
                    ++l->failures;
                    EMIT(EV_FAIL, post->car, s, p, repair);
                }
            }
        }
    }
}

// Машину на посту помыли - пробуем отправить её дальше
static void TryLeave(Line *l, int s, int p) {
    Post *post = &l->stage[s].post[p];
    int c = post->car;
    Car *car = &l->cars[c];
    int next = CarNextStage(car, l->cfg);

    if (next >= 0 && !CanAccept(l, next)) {
        // дальше некуда: стоим на посту и держим его занятым
        if (!car->blocked) {
            car->blocked = 1;
            EMIT(EV_BLOCKED, c, s, p, next);
        }
        return;
    }
    PostRelease(post);
    car->blocked = 0;
    l->moved = 1;

    if (next < 0) {
        car->place = CAR_DONE;
        car->finished = l->now;
        --l->inside;
        ++l->served;
        EMIT(EV_LEAVE, c, s, p, 0);
        EMIT(EV_DONE, c, s, -1, car->finished - car->arrived);
        return;
    }
    car->step++;
    Place(l, c, next);
    EMIT(EV_LEAVE, c, s, p, 0);
    EMIT(EV_MOVE, c, next, -1, s);
    Announce(l, c);
}

static void SweepStage(Line *l, int s) {
    Stage *st = &l->stage[s];
    int posts = l->cfg->stage[s].posts;

    for (int p = 0; p < posts; ++p) {
        Post *post = &st->post[p];
        if (PostStateOf(post) == POST_BUSY && PostWork(post)) {
            EMIT(EV_OP_END, post->car, s, p, 0);
        }
        if (PostStateOf(post) == POST_BLOCKED) {
            TryLeave(l, s, p);
        }
    }
    // освободившиеся посты берут следующих из очереди
    for (int p = 0; p < posts && st->queue.count > 0; ++p) {
        if (PostStateOf(&st->post[p]) == POST_IDLE) {
            int c = BufferPopBest(&st->queue, Score, l);
            StartOnPost(l, c, s, p);
            Announce(l, c);
        }
    }
}

static void Arrive(Line *l) {
    if (l->carCount == l->carCap) {
        l->carCap = l->carCap ? l->carCap * 2 : 64;
        l->cars = CheckedRealloc(l->cars, (size_t)l->carCap * sizeof(Car));
    }
    int c = l->carCount++;
    Car *car = &l->cars[c];
    CarInit(car, l->cfg, l->now);
    int first = car->stage;

    if (CanAccept(l, first)) {
        ++l->inside;
        Place(l, c, first);
        EMIT(EV_ARRIVE, c, -1, -1, 0);
        EMIT(EV_PROGRAM, c, -1, -1, car->program);
        Announce(l, c);
    } else {
        car->place = CAR_REJECTED;
        ++l->rejected;
        EMIT(EV_ARRIVE, c, -1, -1, 0);
        EMIT(EV_PROGRAM, c, -1, -1, car->program);
        EMIT(EV_REJECT, c, first, -1, l->stage[first].queue.count);
    }
    l->nextArrival = l->now + RngRange(l->cfg->arrMin, l->cfg->arrMax);
    if (l->cfg->cars > 0 && l->carCount >= l->cfg->cars) {
        Close(l);
    }
}

// Может ли на линии ещё что-то сдвинуться: кто-то моется или чинят пост,
// который кому-то нужен. Ремонт пустого поста, которого никто не ждёт,
// не считается - иначе при частых поломках тупик находился бы через недели.
static int AnyProgressPossible(const Line *l) {
    const Config *cfg = l->cfg;
    int wanted[MAX_STAGES] = {0};  // куда хотят уехать уже помытые машины
    for (int s = 0; s < cfg->stages; ++s) {
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            const Post *post = &l->stage[s].post[p];
            if (post->car >= 0 && post->left == 0 && CarNextStage(&l->cars[post->car], cfg) >= 0) {
                wanted[CarNextStage(&l->cars[post->car], cfg)] = 1;
            }
        }
    }
    for (int s = 0; s < cfg->stages; ++s) {
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            const Post *post = &l->stage[s].post[p];
            PostState state = PostStateOf(post);
            if (state == POST_BUSY) {
                return 1;
            }
            if (state == POST_BROKEN &&
                (post->car >= 0 || l->stage[s].queue.count > 0 || wanted[s])) {
                return 1;
            }
        }
    }
    return 0;
}

// Пустят ли на въезд хоть кого-нибудь (пустой пост, даже сломанный, тоже считается)
static int EntranceMayAccept(const Line *l) {
    const Config *cfg = l->cfg;
    for (int i = 0; i < cfg->programs; ++i) {
        int s = cfg->program[i].route[0];
        if (BufferHasRoom(&l->stage[s].queue, 0)) {
            return 1;
        }
        for (int p = 0; p < cfg->stage[s].posts; ++p) {
            if (l->stage[s].post[p].car < 0) {
                return 1;
            }
        }
    }
    return 0;
}

// Для сообщения о тупике: стадия без постов, в которую все упёрлись
static int FindStallCause(const Line *l) {
    const Config *cfg = l->cfg;
    for (int c = 0; c < l->carCount; ++c) {
        const Car *car = &l->cars[c];
        int target = -1;
        if (car->place == CAR_QUEUE) {
            target = car->stage;
        }
        if (car->place == CAR_POST && car->blocked) {
            target = CarNextStage(car, cfg);
        }
        if (target >= 0 && cfg->stage[target].posts == 0) {
            return target;
        }
    }
    return -1;
}

static void CheckEnd(Line *l) {
    if (!l->open && l->inside == 0) {
        l->status = LINE_FINISHED;
    } else if (l->inside > 0 && !l->moved && !AnyProgressPossible(l) &&
               !(l->open && EntranceMayAccept(l))) {
        // Никто не сдвинулся и уже не сдвинется. Но пока въезд открыт и пускает
        // машины, мойка ещё работает для остальных программ, так что тупик
        // объявляем, только когда встал и въезд (или он уже закрыт).
        l->status = LINE_STALLED;
        EMIT(EV_STALL, -1, FindStallCause(l), -1, l->inside);
    }
}

void LineTick(Line *l) {
    if (l->status != LINE_RUNNING) {
        return;
    }
    l->moved = 0;
    if (l->open && l->cfg->day > 0 && l->now >= l->cfg->day) {
        Close(l);
    }
    UpdateEquipment(l);
    for (int s = l->cfg->stages - 1; s >= 0; --s) {
        SweepStage(l, s);
    }
    if (l->open && l->now >= l->nextArrival) {
        Arrive(l);
    }
    CheckEnd(l);
    EMIT(EV_TICK, -1, -1, -1, 0);
    ++l->now;
}

void LineInterrupt(Line *l) {
    l->status = LINE_INTERRUPTED;
    EMIT(EV_INTERRUPT, -1, -1, -1, l->inside);
}
