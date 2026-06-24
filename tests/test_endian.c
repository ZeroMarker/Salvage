#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "utils/endian.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_read_le16(void) {
    TEST("read_le16");
    uint8_t data[] = {0x34, 0x12};
    uint16_t val = read_le16(data);
    if (val == 0x1234) PASS(); else FAIL();
}

static void test_read_le32(void) {
    TEST("read_le32");
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
    uint32_t val = read_le32(data);
    if (val == 0x12345678) PASS(); else FAIL();
}

static void test_read_le64(void) {
    TEST("read_le64");
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint64_t val = read_le64(data);
    if (val == 0x0807060504030201ULL) PASS(); else FAIL();
}

static void test_write_le16(void) {
    TEST("write_le16");
    uint8_t data[2];
    write_le16(data, 0x1234);
    if (data[0] == 0x34 && data[1] == 0x12) PASS(); else FAIL();
}

static void test_write_le32(void) {
    TEST("write_le32");
    uint8_t data[4];
    write_le32(data, 0x12345678);
    if (data[0] == 0x78 && data[1] == 0x56 && data[2] == 0x34 && data[3] == 0x12) PASS(); else FAIL();
}

static void test_bswap16(void) {
    TEST("bswap16");
    if (bswap16(0x1234) == 0x3412) PASS(); else FAIL();
}

static void test_bswap32(void) {
    TEST("bswap32");
    if (bswap32(0x12345678) == 0x78563412) PASS(); else FAIL();
}

int main(void) {
    printf("Endian Tests:\n");
    
    test_read_le16();
    test_read_le32();
    test_read_le64();
    test_write_le16();
    test_write_le32();
    test_bswap16();
    test_bswap32();
    
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
