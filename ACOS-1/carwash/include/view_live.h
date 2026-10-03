#ifndef VIEW_LIVE_H
#define VIEW_LIVE_H

// Живая схема линии в терминале, перерисовывается раз в минуту модели

#include "events.h"

void ViewLiveInit(int fd);
void ViewLiveListener(const Event *e, const struct Line *line);
void ViewLiveEnd(void);  // вернуть курсор

#endif
