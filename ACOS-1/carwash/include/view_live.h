/**
 * @file view_live.h
 * @brief Живая схема линии в терминале, перерисовывается раз в минуту модели.
 */
#ifndef VIEW_LIVE_H
#define VIEW_LIVE_H

#include "events.h"

void ViewLiveInit(int fd);
void ViewLiveListener(const Event *e, const struct Line *line);
void ViewLiveEnd(void);  ///< вернуть курсор

#endif
