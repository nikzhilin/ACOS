/**
 * @file view_log.h
 * @brief Лента событий на экран и в журнал.
 */
#ifndef VIEW_LOG_H
#define VIEW_LOG_H

#include "events.h"

/**
 * @param screenFd  куда печатать ленту, -1 - не печатать (работает живая схема)
 * @param color     раскрашивать ли строки на экране
 * @param journalFd файл журнала, -1 - без журнала
 */
void ViewLogInit(int screenFd, int color, int journalFd);
void ViewLogListener(const Event *e, const struct Line *line);

#endif
