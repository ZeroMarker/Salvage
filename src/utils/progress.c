#include "progress.h"
#include <stdio.h>
#include <string.h>

void progress_init(progress_t *prog, uint64_t total, int width) {
    prog->width = width;
    prog->current = 0;
    prog->total = total;
    prog->last_percent = -1;
}

void progress_update(progress_t *prog, uint64_t current) {
    prog->current = current;
    int percent = prog->total > 0 ? (int)((current * 100) / prog->total) : 0;
    if (percent > 100) percent = 100;
    
    if (percent == prog->last_percent) return;
    prog->last_percent = percent;
    
    int filled = (percent * prog->width) / 100;
    
    fprintf(stderr, "\r  [");
    for (int i = 0; i < prog->width; i++) {
        if (i < filled) fprintf(stderr, "█");
        else fprintf(stderr, "░");
    }
    fprintf(stderr, "] %3d%%", percent);
    fflush(stderr);
}

void progress_finish(progress_t *prog) {
    prog->current = prog->total;
    prog->last_percent = 100;
    
    fprintf(stderr, "\r  [");
    for (int i = 0; i < prog->width; i++) {
        fprintf(stderr, "█");
    }
    fprintf(stderr, "] 100%%\n");
    fflush(stderr);
}
