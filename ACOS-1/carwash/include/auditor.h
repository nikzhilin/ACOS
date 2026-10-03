#ifndef AUDITOR_H
#define AUDITOR_H

// Аудитор проверяет инварианты задачи после каждого события:
//   I1 на посту не больше одной машины
//   I2 машина ровно в одном месте (очередь или пост)
//   I3 стадии идут в порядке программы
//   I4 сломанный пост не начинает операцию
//   I5 очередь не переполнена
// Порядок стадий и поломки он отслеживает сам по событиям, а не берёт у line.c.

#include "events.h"

void AuditorInit(int errFd);
void AuditorListener(const Event *e, const struct Line *line);
void AuditorPrint(int fd);
void AuditorFree(void);

long AuditorChecks(void);
long AuditorViolations(void);

#endif
