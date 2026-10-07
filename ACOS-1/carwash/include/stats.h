/**
 * @file stats.h
 * @brief Итоговая статистика дня: сколько вымыто, время на мойке, загрузка стадий.
 */
#ifndef STATS_H
#define STATS_H

#include "events.h"

void StatsListener(const Event *e, const struct Line *line);
void StatsPrint(int fd, const struct Line *line);

#endif
