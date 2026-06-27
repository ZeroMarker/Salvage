#include <stdio.h>
#include <string.h>
#include "fs/ntfs/ntfs.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-45s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_boot_struct_size(void) {
    TEST("ntfs_boot_t struct size > 0");
    if (sizeof(ntfs_boot_t) > 0) PASS(); else FAIL();
}

static void test_mft_record_header_size(void) {
    TEST("mft_record_header_t struct size > 0");
    if (sizeof(mft_record_header_t) > 0) PASS(); else FAIL();
}

static void test_attr_header_size(void) {
    TEST("attr_header_t struct size > 0");
    if (sizeof(attr_header_t) > 0) PASS(); else FAIL();
}

static void test_attr_resident_size(void) {
    TEST("attr_resident_t struct size > 0");
    if (sizeof(attr_resident_t) > 0) PASS(); else FAIL();
}

static void test_attr_non_resident_size(void) {
    TEST("attr_non_resident_t struct size > 0");
    if (sizeof(attr_non_resident_t) > 0) PASS(); else FAIL();
}

static void test_attr_filename_size(void) {
    TEST("attr_filename_t struct size > 0");
    if (sizeof(attr_filename_t) > 0) PASS(); else FAIL();
}

static void test_attr_standard_info_size(void) {
    TEST("attr_standard_info_t struct size > 0");
    if (sizeof(attr_standard_info_t) > 0) PASS(); else FAIL();
}

static void test_valid_mft_record(void) {
    TEST("ntfs_is_valid_mft_record with FILE magic");
    uint8_t record[1024];
    memset(record, 0, sizeof(record));
    memcpy(record, "FILE", 4);
    if (ntfs_is_valid_mft_record(record)) PASS(); else FAIL();
}

static void test_invalid_mft_record(void) {
    TEST("ntfs_is_valid_mft_record with bad magic");
    uint8_t record[1024];
    memset(record, 0, sizeof(record));
    memcpy(record, "BAAD", 4);
    if (!ntfs_is_valid_mft_record(record)) PASS(); else FAIL();
}

static void test_deleted_record_in_use(void) {
    TEST("ntfs_is_deleted_record: in-use record");
    uint8_t record[1024];
    memset(record, 0, sizeof(record));
    memcpy(record, "FILE", 4);
    record[22] = 0x01;  // flags: in-use
    if (!ntfs_is_deleted_record(record)) PASS(); else FAIL();
}

static void test_deleted_record_freed(void) {
    TEST("ntfs_is_deleted_record: deleted record");
    uint8_t record[1024];
    memset(record, 0, sizeof(record));
    memcpy(record, "FILE", 4);
    record[22] = 0x00;  // flags: not in-use
    if (ntfs_is_deleted_record(record)) PASS(); else FAIL();
}

static void test_deleted_record_directory(void) {
    TEST("ntfs_is_deleted_record: deleted directory");
    uint8_t record[1024];
    memset(record, 0, sizeof(record));
    memcpy(record, "FILE", 4);
    record[22] = 0x02;  // flags: directory but not in-use
    if (ntfs_is_deleted_record(record)) PASS(); else FAIL();
}

static void test_constants(void) {
    TEST("NTFS magic and constants");
    if (strcmp(NTFS_MAGIC, "NTFS    ") == 0 &&
        NTFS_SECTOR_SIZE == 512 &&
        NTFS_MFT_RECORD_SIZE == 1024 &&
        MFT_FLAG_IN_USE == 0x01 &&
        MFT_FLAG_DIRECTORY == 0x02 &&
        MFT_ENTRY_FIRST_USER == 16 &&
        ATTR_TYPE_END == 0xFFFFFFFF) {
        PASS();
    } else {
        FAIL();
    }
}

static void test_attribute_types(void) {
    TEST("NTFS attribute type constants");
    if (ATTR_TYPE_STANDARD_INFO == 0x10 &&
        ATTR_TYPE_FILE_NAME == 0x30 &&
        ATTR_TYPE_DATA == 0x80 &&
        ATTR_TYPE_END == 0xFFFFFFFF) {
        PASS();
    } else {
        FAIL();
    }
}

int main(void) {
    printf("NTFS Tests:\n");

    test_boot_struct_size();
    test_mft_record_header_size();
    test_attr_header_size();
    test_attr_resident_size();
    test_attr_non_resident_size();
    test_attr_filename_size();
    test_attr_standard_info_size();
    test_valid_mft_record();
    test_invalid_mft_record();
    test_deleted_record_in_use();
    test_deleted_record_freed();
    test_deleted_record_directory();
    test_constants();
    test_attribute_types();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
