/**
 * @file post.h
 * @brief Пост мойки: моет одну машину за раз, иногда ломается.
 *
 * Состояние поста отдельно не храним, а считаем из полей,
 * чтобы оно не могло разойтись с ними.
 */
#ifndef POST_H
#define POST_H

typedef enum {
    POST_IDLE,
    POST_BUSY,
    POST_BLOCKED,  ///< машина помыта, но уехать ей некуда
    POST_BROKEN,
} PostState;

typedef struct {
    int car;       ///< -1, если пусто
    int left;      ///< минут до конца операции
    int duration;  ///< сколько всего длится операция (для прогресс-бара)
    int repair;    ///< минут до конца ремонта, 0 - исправен
} Post;

void PostInit(Post *p);
PostState PostStateOf(const Post *p);

/// Ставит машину на пост. Пост должен быть свободен и исправен.
void PostStart(Post *p, int car, int duration);

/// Снимает машину с поста. @return номер машины
int PostRelease(Post *p);

/// @return время ремонта, если пост сломался, иначе 0
int PostTryBreak(Post *p, double prob, int rmin, int rmax);

/// Минута ремонта. @return 1 в ту минуту, когда ремонт закончился
int PostRepairStep(Post *p);

/// Минута работы. @return 1 в ту минуту, когда операция закончилась
int PostWork(Post *p);

#endif
