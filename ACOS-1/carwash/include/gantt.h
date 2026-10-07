/**
 * @file gantt.h
 * @brief Диаграмма Ганта по постам и поиск узкого места.
 *
 * Каждую минуту запоминаем состояние каждого поста, в конце печатаем ленту:
 * `#` моет, `=` ждёт, `X` ремонт, `.` простой.
 */
#ifndef GANTT_H
#define GANTT_H

#include "events.h"

void GanttListener(const Event *e, const struct Line *line);

/// @param color 1 - раскрасить ленту ANSI-цветами
void GanttPrint(int fd, const struct Line *line, int color);
void GanttFree(void);

#endif
