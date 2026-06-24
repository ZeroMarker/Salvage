#include <stdio.h>
#include <string.h>
#include "partition/partition.h"
#include "utils/log.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_fs_type_names(void) {
    TEST("fs_type_name");
    if (strcmp(fs_type_name(FS_TYPE_NTFS), "NTFS") == 0 &&
        strcmp(fs_type_name(FS_TYPE_FAT32), "FAT32") == 0 &&
        strcmp(fs_type_name(FS_TYPE_EXT4), "ext4") == 0) {
        PASS();
    } else {
        FAIL();
    }
}

static void test_mbr_type_names(void) {
    TEST("mbr_type_name");
    if (strcmp(mbr_type_name(0x07), "NTFS/exFAT") == 0 &&
        strcmp(mbr_type_name(0x83), "Linux") == 0 &&
        strcmp(mbr_type_name(0x00), "Empty") == 0) {
        PASS();
    } else {
        FAIL();
    }
}

int main(void) {
    printf("Partition Tests:\n");
    
    test_fs_type_names();
    test_mbr_type_names();
    
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
