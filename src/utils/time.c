#include "time.h"
#include <stdio.h>
#include <time.h>

// NTFS epoch: 1601-01-01 00:00:00
// Unix epoch: 1970-01-01 00:00:00
// Difference: 11644473600 seconds
#define NTFS_EPOCH_DIFF 11644473600ULL
#define HNS_PER_SECOND  10000000ULL

int64_t ntfs_time_to_unix(uint64_t ntfs_time) {
    if (ntfs_time == 0) return 0;
    return (int64_t)(ntfs_time / HNS_PER_SECOND) - NTFS_EPOCH_DIFF;
}

void format_time(uint64_t ntfs_time, char *buf, int buf_size) {
    if (ntfs_time == 0) {
        snprintf(buf, buf_size, "N/A");
        return;
    }
    
    int64_t unix_time = ntfs_time_to_unix(ntfs_time);
    time_t t = (time_t)unix_time;
    struct tm *tm = gmtime(&t);
    
    if (tm) {
        strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm);
    } else {
        snprintf(buf, buf_size, "Invalid");
    }
}
