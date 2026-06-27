#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "fs/fat/fat.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_parse_short_name(void) {
    TEST("fat_parse_short_name");
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, "TEST    ", 8);
    memcpy(entry.ext, "TXT", 3);

    char name[13];
    fat_parse_short_name(&entry, name, sizeof(name));
    if (strcmp(name, "TEST.TXT") == 0) PASS(); else FAIL();
}

static void test_parse_short_name_no_ext(void) {
    TEST("fat_parse_short_name (no ext)");
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, "README  ", 8);
    memset(entry.ext, ' ', 3);

    char name[13];
    fat_parse_short_name(&entry, name, sizeof(name));
    if (strcmp(name, "README") == 0) PASS(); else FAIL();
}

static void test_parse_short_name_long(void) {
    TEST("fat_parse_short_name (long)");
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, "LONGFILE", 8);
    memcpy(entry.ext, "DAT", 3);

    char name[13];
    fat_parse_short_name(&entry, name, sizeof(name));
    if (strcmp(name, "LONGFILE.DAT") == 0) PASS(); else FAIL();
}

static void test_is_deleted(void) {
    TEST("fat_is_deleted_entry (deleted)");
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.name[0] = 0xE5;
    if (fat_is_deleted_entry(&entry)) PASS(); else FAIL();
}

static void test_is_not_deleted(void) {
    TEST("fat_is_deleted_entry (normal)");
    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.name[0] = 'T';
    if (!fat_is_deleted_entry(&entry)) PASS(); else FAIL();
}

static void test_boot_struct_size(void) {
    TEST("fat_boot_t struct size");
    if (sizeof(fat_boot_t) == 512) PASS(); else FAIL();
}

static void test_dir_entry_size(void) {
    TEST("fat_dir_entry_t struct size");
    if (sizeof(fat_dir_entry_t) == 32) PASS(); else FAIL();
}

static void test_lfn_entry_size(void) {
    TEST("fat_lfn_entry_t struct size");
    if (sizeof(fat_lfn_entry_t) == 32) PASS(); else FAIL();
}

int main(void) {
    printf("FAT32 Tests:\n");

    test_parse_short_name();
    test_parse_short_name_no_ext();
    test_parse_short_name_long();
    test_is_deleted();
    test_is_not_deleted();
    test_boot_struct_size();
    test_dir_entry_size();
    test_lfn_entry_size();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
