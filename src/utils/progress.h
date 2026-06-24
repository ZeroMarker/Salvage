#ifndef SALVAGE_PROGRESS_H
#define SALVAGE_PROGRESS_H

#include <stdint.h>

typedef struct {
    int width;          // Progress bar width in characters
    uint64_t current;
    uint64_t total;
    int last_percent;
} progress_t;

void progress_init(progress_t *prog, uint64_t total, int width);
void progress_update(progress_t *prog, uint64_t current);
void progress_finish(progress_t *prog);

#endif // SALVAGE_PROGRESS_H
