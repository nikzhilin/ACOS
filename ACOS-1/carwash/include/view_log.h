#ifndef VIEW_LOG_H
#define VIEW_LOG_H

// Лента событий: на экран (если screenFd >= 0) и в журнал (если journalFd >= 0)

#include "events.h"

void ViewLogInit(int screenFd, int color, int journalFd);
void ViewLogListener(const Event *e, const struct Line *line);

#endif
