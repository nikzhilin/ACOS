#ifndef POST_H
#define POST_H

// Пост моет одну машину за раз. Состояние поста отдельно не храним,
// а считаем из полей, чтобы оно не могло разойтись с ними.

typedef enum {
    POST_IDLE,
    POST_BUSY,
    POST_BLOCKED,  // машина помыта, но уехать ей некуда
    POST_BROKEN,
} PostState;

typedef struct {
    int car;       // -1, если пусто
    int left;      // минут до конца операции
    int duration;  // сколько всего длится операция (для прогресс-бара)
    int repair;    // минут до конца ремонта, 0 - исправен
} Post;

void PostInit(Post *p);
PostState PostStateOf(const Post *p);

void PostStart(Post *p, int car, int duration);
int PostRelease(Post *p);  // возвращает номер машины

// возвращает время ремонта, если пост сломался, иначе 0
int PostTryBreak(Post *p, double prob, int rmin, int rmax);

// обе возвращают 1 в ту минуту, когда ремонт / операция закончились
int PostRepairStep(Post *p);
int PostWork(Post *p);

#endif
