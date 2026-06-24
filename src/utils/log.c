#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static log_level_t g_log_level = LOG_LEVEL_INFO;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "NONE"
};

static const char *level_colors[] = {
    "\033[90m",  // TRACE - gray
    "\033[36m",  // DEBUG - cyan
    "\033[32m",  // INFO  - green
    "\033[33m",  // WARN  - yellow
    "\033[31m",  // ERROR - red
    "\033[35m",  // FATAL - magenta
    "\033[0m",   // NONE
};

void log_set_level(log_level_t level) {
    g_log_level = level;
}

log_level_t log_get_level(void) {
    return g_log_level;
}

void log_log(log_level_t level, const char *file, int line, const char *fmt, ...) {
    if (level < g_log_level) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);

    // Extract filename from path
    const char *filename = strrchr(file, '/');
    if (!filename) filename = strrchr(file, '\\');
    if (filename) filename++;
    else filename = file;

    fprintf(stderr, "%s%s %-5s\033[0m \033[90m%s:%d\033[0m ",
            level_colors[level], time_buf, level_names[level],
            filename, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
