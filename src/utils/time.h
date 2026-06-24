#ifndef SALVAGE_TIME_H
#define SALVAGE_TIME_H

#include <stdint.h>

// Convert NTFS timestamp (100-nanosecond intervals since 1601-01-01) to Unix time
int64_t ntfs_time_to_unix(uint64_t ntfs_time);

// Format timestamp to string
void format_time(uint64_t ntfs_time, char *buf, int buf_size);

#endif // SALVAGE_TIME_H
