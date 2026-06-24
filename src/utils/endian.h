#ifndef SALVAGE_ENDIAN_H
#define SALVAGE_ENDIAN_H

#include <stdint.h>

// Check endianness at compile time
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

// Byte swap functions
static inline uint16_t bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

static inline uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

static inline uint64_t bswap64(uint64_t x) {
    return ((x >> 56) & 0x00000000000000FFULL) |
           ((x >> 40) & 0x000000000000FF00ULL) |
           ((x >> 24) & 0x0000000000FF0000ULL) |
           ((x >>  8) & 0x00000000FF000000ULL) |
           ((x <<  8) & 0x000000FF00000000ULL) |
           ((x << 24) & 0x0000FF0000000000ULL) |
           ((x << 40) & 0x00FF000000000000ULL) |
           ((x << 56) & 0xFF00000000000000ULL);
}

// Little-endian to host
static inline uint16_t le16toh(uint16_t x) {
#if IS_BIG_ENDIAN
    return bswap16(x);
#else
    return x;
#endif
}

static inline uint32_t le32toh(uint32_t x) {
#if IS_BIG_ENDIAN
    return bswap32(x);
#else
    return x;
#endif
}

static inline uint64_t le64toh(uint64_t x) {
#if IS_BIG_ENDIAN
    return bswap64(x);
#else
    return x;
#endif
}

// Host to little-endian
static inline uint16_t htole16(uint16_t x) { return le16toh(x); }
static inline uint32_t htole32(uint32_t x) { return le32toh(x); }
static inline uint64_t htole64(uint64_t x) { return le64toh(x); }

// Big-endian to host
static inline uint16_t be16toh(uint16_t x) {
#if IS_BIG_ENDIAN
    return x;
#else
    return bswap16(x);
#endif
}

static inline uint32_t be32toh(uint32_t x) {
#if IS_BIG_ENDIAN
    return x;
#else
    return bswap32(x);
#endif
}

static inline uint64_t be64toh(uint64_t x) {
#if IS_BIG_ENDIAN
    return x;
#else
    return bswap64(x);
#endif
}

// Read little-endian values from byte array
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t read_le64(const uint8_t *p) {
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

// Write little-endian values to byte array
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void write_le64(uint8_t *p, uint64_t v) {
    write_le32(p, (uint32_t)v);
    write_le32(p + 4, (uint32_t)(v >> 32));
}

#endif // SALVAGE_ENDIAN_H
