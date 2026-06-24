#ifndef SALVAGE_STR_H
#define SALVAGE_STR_H

#include <stdint.h>
#include <stddef.h>

// UTF-16LE to UTF-8 conversion
// Returns number of bytes written (excluding null terminator), or -1 on error
int utf16_to_utf8(const uint16_t *src, int src_len, char *dst, int dst_size);

// Safe string copy
void str_copy(char *dst, const char *src, size_t dst_size);

// Convert bytes to human readable string
void format_size(uint64_t bytes, char *buf, size_t buf_size);

#endif // SALVAGE_STR_H
