#include "str.h"
#include <stdio.h>
#include <string.h>

int utf16_to_utf8(const uint16_t *src, int src_len, char *dst, int dst_size) {
    int di = 0;
    for (int si = 0; si < src_len; si++) {
        uint32_t cp = src[si];
        
        // Surrogate pair
        if (cp >= 0xD800 && cp <= 0xDBFF && si + 1 < src_len) {
            uint32_t low = src[si + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                si++;
            }
        }
        
        if (cp < 0x80) {
            if (di + 1 >= dst_size) return -1;
            dst[di++] = (char)cp;
        } else if (cp < 0x800) {
            if (di + 2 >= dst_size) return -1;
            dst[di++] = (char)(0xC0 | (cp >> 6));
            dst[di++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            if (di + 3 >= dst_size) return -1;
            dst[di++] = (char)(0xE0 | (cp >> 12));
            dst[di++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[di++] = (char)(0x80 | (cp & 0x3F));
        } else {
            if (di + 4 >= dst_size) return -1;
            dst[di++] = (char)(0xF0 | (cp >> 18));
            dst[di++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            dst[di++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            dst[di++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    if (di < dst_size) dst[di] = '\0';
    return di;
}

void str_copy(char *dst, const char *src, size_t dst_size) {
    if (dst_size == 0) return;
    size_t i;
    for (i = 0; i < dst_size - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void format_size(uint64_t bytes, char *buf, size_t buf_size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
        unit++;
    }
    
    if (unit == 0) {
        snprintf(buf, buf_size, "%llu B", (unsigned long long)bytes);
    } else {
        snprintf(buf, buf_size, "%.1f %s", size, units[unit]);
    }
}
