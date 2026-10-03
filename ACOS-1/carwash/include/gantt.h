#ifndef GANTT_H
#define GANTT_H

// Диаграмма Ганта: каждую минуту запоминаем состояние каждого поста,
// в конце печатаем ленту # (моет) = (ждёт) X (ремонт) . (простой)

#include "events.h"

void GanttListener(const Event *e, const struct Line *line);
void GanttPrint(int fd, const struct Line *line, int color);
void GanttFree(void);

#endif
