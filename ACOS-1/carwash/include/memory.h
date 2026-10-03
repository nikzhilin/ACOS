#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include <unistd.h>

// realloc, после которого не надо проверять NULL: без памяти всё равно выходим
static inline void *CheckedRealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (p == NULL) {
        static const char msg[] = "carwash: out of memory\n";
        if (write(2, msg, sizeof(msg) - 1) < 0) {
            // печатать уже некуда
        }
        exit(5);
    }
    return p;
}

#endif
