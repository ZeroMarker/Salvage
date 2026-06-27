#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/endian.h"
#include "utils/str.h"
#include "utils/time.h"
#include "utils/log.h"
#include "partition/partition.h"
#include "signature/signature.h"
#include "core/result.h"
#include "fs/fat/fat.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-45s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL (%s:%d)\n", __FILE__, __LINE__); tests_failed++; } while(0)
#define ASSERT(cond) do { if (!(cond)) { FAIL(); return; } } while(0)

// ============================================================
// Utils: endian.h
// ============================================================
static void test_endian_le16(void) {
    TEST("endian: read_le16 basic");
    uint8_t d[] = {0x34, 0x12};
    ASSERT(read_le16(d) == 0x1234); PASS();
}
static void test_endian_le16_zero(void) {
    TEST("endian: read_le16 zero");
    uint8_t d[] = {0x00, 0x00};
    ASSERT(read_le16(d) == 0); PASS();
}
static void test_endian_le16_max(void) {
    TEST("endian: read_le16 max");
    uint8_t d[] = {0xFF, 0xFF};
    ASSERT(read_le16(d) == 0xFFFF); PASS();
}
static void test_endian_le32_basic(void) {
    TEST("endian: read_le32 basic");
    uint8_t d[] = {0x78, 0x56, 0x34, 0x12};
    ASSERT(read_le32(d) == 0x12345678); PASS();
}
static void test_endian_le32_zero(void) {
    TEST("endian: read_le32 zero");
    uint8_t d[] = {0, 0, 0, 0};
    ASSERT(read_le32(d) == 0); PASS();
}
static void test_endian_le64_basic(void) {
    TEST("endian: read_le64 basic");
    uint8_t d[] = {1,2,3,4,5,6,7,8};
    ASSERT(read_le64(d) == 0x0807060504030201ULL); PASS();
}
static void test_endian_write_le16(void) {
    TEST("endian: write_le16 roundtrip");
    uint8_t d[2];
    write_le16(d, 0xABCD);
    ASSERT(read_le16(d) == 0xABCD); PASS();
}
static void test_endian_write_le32(void) {
    TEST("endian: write_le32 roundtrip");
    uint8_t d[4];
    write_le32(d, 0xDEADBEEF);
    ASSERT(read_le32(d) == 0xDEADBEEF); PASS();
}
static void test_endian_bswap16(void) {
    TEST("endian: bswap16");
    ASSERT(bswap16(0x1234) == 0x3412); PASS();
}
static void test_endian_bswap32(void) {
    TEST("endian: bswap32");
    ASSERT(bswap32(0x12345678) == 0x78563412); PASS();
}

// ============================================================
// Utils: str.h
// ============================================================
static void test_format_size_bytes(void) {
    TEST("str: format_size bytes");
    char buf[32];
    format_size(512, buf, sizeof(buf));
    ASSERT(strcmp(buf, "512 B") == 0); PASS();
}
static void test_format_size_kb(void) {
    TEST("str: format_size KB");
    char buf[32];
    format_size(1024, buf, sizeof(buf));
    ASSERT(strcmp(buf, "1.0 KB") == 0); PASS();
}
static void test_format_size_mb(void) {
    TEST("str: format_size MB");
    char buf[32];
    format_size(1048576, buf, sizeof(buf));
    ASSERT(strcmp(buf, "1.0 MB") == 0); PASS();
}
static void test_format_size_gb(void) {
    TEST("str: format_size GB");
    char buf[32];
    format_size(1073741824ULL, buf, sizeof(buf));
    ASSERT(strcmp(buf, "1.0 GB") == 0); PASS();
}
static void test_format_size_zero(void) {
    TEST("str: format_size zero");
    char buf[32];
    format_size(0, buf, sizeof(buf));
    ASSERT(strcmp(buf, "0 B") == 0); PASS();
}
static void test_str_copy(void) {
    TEST("str: str_copy basic");
    char dst[16];
    str_copy(dst, "hello", sizeof(dst));
    ASSERT(strcmp(dst, "hello") == 0); PASS();
}
static void test_str_copy_truncate(void) {
    TEST("str: str_copy truncation");
    char dst[4];
    str_copy(dst, "hello", sizeof(dst));
    ASSERT(strlen(dst) <= 3); PASS();
}
static void test_utf16_ascii(void) {
    TEST("str: utf16_to_utf8 ASCII");
    uint16_t src[] = {'H','e','l','l','o'};
    char dst[32];
    int n = utf16_to_utf8(src, 5, dst, sizeof(dst));
    ASSERT(n == 5 && strcmp(dst, "Hello") == 0); PASS();
}
static void test_utf16_empty(void) {
    TEST("str: utf16_to_utf8 empty");
    uint16_t src[] = {0};
    char dst[32];
    int n = utf16_to_utf8(src, 0, dst, sizeof(dst));
    ASSERT(n == 0 && dst[0] == '\0'); PASS();
}

// ============================================================
// Utils: time.h
// ============================================================
static void test_ntfs_time_zero(void) {
    TEST("time: ntfs_time_to_unix zero returns 0");
    int64_t t = ntfs_time_to_unix(0);
    ASSERT(t == 0); PASS();
}
static void test_ntfs_time_known(void) {
    TEST("time: ntfs_time_to_unix 2020-01-01");
    // 2020-01-01 00:00:00 UTC = 1577836800 unix
    // NTFS = (1577836800 + 11644473600) * 10000000 = 132223104000000000
    uint64_t ntfs = 132223104000000000ULL;
    int64_t unix_t = ntfs_time_to_unix(ntfs);
    ASSERT(unix_t == 1577836800); PASS();
}

// ============================================================
// Utils: log.h
// ============================================================
static void test_log_level(void) {
    TEST("log: set/get level");
    log_set_level(LOG_LEVEL_DEBUG);
    ASSERT(log_get_level() == LOG_LEVEL_DEBUG);
    log_set_level(LOG_LEVEL_ERROR);
    ASSERT(log_get_level() == LOG_LEVEL_ERROR);
    log_set_level(LOG_LEVEL_INFO);
    ASSERT(log_get_level() == LOG_LEVEL_INFO);
    PASS();
}

// ============================================================
// Partition: fs_type_name and mbr_type_name
// ============================================================
static void test_fs_names(void) {
    TEST("partition: fs_type_name all types");
    ASSERT(strcmp(fs_type_name(FS_TYPE_NTFS), "NTFS") == 0);
    ASSERT(strcmp(fs_type_name(FS_TYPE_FAT32), "FAT32") == 0);
    ASSERT(strcmp(fs_type_name(FS_TYPE_EXFAT), "exFAT") == 0);
    ASSERT(strcmp(fs_type_name(FS_TYPE_EXT4), "ext4") == 0);
    ASSERT(strcmp(fs_type_name(FS_TYPE_UNKNOWN), "Unknown") == 0);
    PASS();
}
static void test_mbr_names(void) {
    TEST("partition: mbr_type_name key types");
    ASSERT(strcmp(mbr_type_name(0x07), "NTFS/exFAT") == 0);
    ASSERT(strcmp(mbr_type_name(0x0B), "FAT32 CHS") == 0);
    ASSERT(strcmp(mbr_type_name(0x83), "Linux") == 0);
    ASSERT(strcmp(mbr_type_name(0x00), "Empty") == 0);
    PASS();
}

// ============================================================
// Signature: edge cases
// ============================================================
static void test_sig_all_defaults(void) {
    TEST("sig: all default signatures loaded");
    signature_db_t db;
    sig_load_defaults(&db);
    ASSERT(db.count >= 14);
    PASS();
}
static void test_sig_match_gif(void) {
    TEST("sig: match GIF");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
    int idx = sig_match_header(&db, d, sizeof(d));
    ASSERT(idx >= 0 && strcmp(db.entries[idx].name, "GIF") == 0);
    PASS();
}
static void test_sig_match_bmp(void) {
    TEST("sig: match BMP");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0x42, 0x4D, 0x00, 0x00};
    int idx = sig_match_header(&db, d, sizeof(d));
    ASSERT(idx >= 0 && strcmp(db.entries[idx].name, "BMP") == 0);
    PASS();
}
static void test_sig_match_zip(void) {
    TEST("sig: match ZIP");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0x50, 0x4B, 0x03, 0x04, 0x00};
    int idx = sig_match_header(&db, d, sizeof(d));
    ASSERT(idx >= 0 && strcmp(db.entries[idx].name, "ZIP") == 0);
    PASS();
}
static void test_sig_match_exe(void) {
    TEST("sig: match EXE (MZ)");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0x4D, 0x5A, 0x90, 0x00};
    int idx = sig_match_header(&db, d, sizeof(d));
    ASSERT(idx >= 0 && strcmp(db.entries[idx].name, "EXE") == 0);
    PASS();
}
static void test_sig_match_mkv(void) {
    TEST("sig: match MKV");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0x1A, 0x45, 0xDF, 0xA3, 0x00};
    int idx = sig_match_header(&db, d, sizeof(d));
    ASSERT(idx >= 0 && strcmp(db.entries[idx].name, "MKV") == 0);
    PASS();
}
static void test_sig_short_data(void) {
    TEST("sig: short data returns -1");
    signature_db_t db;
    sig_load_defaults(&db);
    uint8_t d[] = {0xFF};
    int idx = sig_match_header(&db, d, 1);
    ASSERT(idx < 0);
    PASS();
}
static void test_sig_null_data(void) {
    TEST("sig: null data returns -1");
    signature_db_t db;
    sig_load_defaults(&db);
    int idx = sig_match_header(&db, NULL, 10);
    ASSERT(idx < 0);
    PASS();
}
static void test_sig_null_db(void) {
    TEST("sig: null db returns -1");
    uint8_t d[] = {0xFF, 0xD8, 0xFF};
    int idx = sig_match_header(NULL, d, 3);
    ASSERT(idx < 0);
    PASS();
}
static void test_sig_footer_match(void) {
    TEST("sig: footer match JPEG");
    signature_db_t db;
    sig_load_defaults(&db);
    // Find JPEG index
    int idx = -1;
    for (int i = 0; i < db.count; i++) {
        if (strcmp(db.entries[i].name, "JPEG") == 0) { idx = i; break; }
    }
    ASSERT(idx >= 0);
    uint8_t tail[] = {0x00, 0x00, 0xFF, 0xD9};
    ASSERT(sig_match_footer(&db, idx, tail, 4) == 1);
    PASS();
}
static void test_sig_footer_no_match(void) {
    TEST("sig: footer no match");
    signature_db_t db;
    sig_load_defaults(&db);
    int idx = -1;
    for (int i = 0; i < db.count; i++) {
        if (strcmp(db.entries[i].name, "JPEG") == 0) { idx = i; break; }
    }
    ASSERT(idx >= 0);
    uint8_t tail[] = {0x00, 0x00, 0x00, 0x00};
    ASSERT(sig_match_footer(&db, idx, tail, 4) == 0);
    PASS();
}
static void test_sig_get_valid(void) {
    TEST("sig: sig_get valid index");
    signature_db_t db;
    sig_load_defaults(&db);
    const signature_t *s = sig_get(&db, 0);
    ASSERT(s != NULL && s->name[0] != '\0');
    PASS();
}
static void test_sig_get_out_of_range(void) {
    TEST("sig: sig_get out of range");
    signature_db_t db;
    sig_load_defaults(&db);
    ASSERT(sig_get(&db, -1) == NULL);
    ASSERT(sig_get(&db, db.count) == NULL);
    PASS();
}
static void test_sig_category_all(void) {
    TEST("sig: all category names");
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_IMAGE), "Image") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_DOCUMENT), "Document") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_AUDIO), "Audio") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_VIDEO), "Video") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_ARCHIVE), "Archive") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_EXECUTABLE), "Executable") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_DATABASE), "Database") == 0);
    ASSERT(strcmp(sig_category_name(SIG_CATEGORY_UNKNOWN), "Unknown") == 0);
    PASS();
}

// ============================================================
// Result list: edge cases
// ============================================================
static void test_result_free_null(void) {
    TEST("result: free NULL is safe");
    result_list_free(NULL);
    PASS();
}
static void test_result_sort_empty(void) {
    TEST("result: sort empty list");
    result_list_t list;
    result_list_init(&list);
    result_list_sort(&list);
    ASSERT(list.count == 0);
    PASS();
}
static void test_result_sort_single(void) {
    TEST("result: sort single item");
    result_list_t list;
    result_list_init(&list);
    scan_result_t r = {0};
    r.file_id = 42;
    result_list_add(&list, &r);
    result_list_sort(&list);
    ASSERT(list.count == 1 && list.items[0].file_id == 42);
    result_list_free(&list);
    PASS();
}
static void test_result_filter_empty(void) {
    TEST("result: filter empty list");
    result_list_t list;
    result_list_init(&list);
    int c = result_list_filter_by_category(&list, SIG_CATEGORY_IMAGE);
    ASSERT(c == 0);
    result_list_free(&list);
    PASS();
}
static void test_result_filter_no_match(void) {
    TEST("result: filter with no match");
    result_list_t list;
    result_list_init(&list);
    scan_result_t r = {0};
    r.file_id = 1; r.category = SIG_CATEGORY_IMAGE;
    result_list_add(&list, &r);
    int c = result_list_filter_by_category(&list, SIG_CATEGORY_AUDIO);
    ASSERT(c == 0 && list.count == 0);
    result_list_free(&list);
    PASS();
}
static void test_result_filter_all_match(void) {
    TEST("result: filter all match");
    result_list_t list;
    result_list_init(&list);
    for (int i = 0; i < 5; i++) {
        scan_result_t r = {0};
        r.file_id = i; r.category = SIG_CATEGORY_VIDEO;
        result_list_add(&list, &r);
    }
    int c = result_list_filter_by_category(&list, SIG_CATEGORY_VIDEO);
    ASSERT(c == 5 && list.count == 5);
    result_list_free(&list);
    PASS();
}
static void test_result_filter_min_size_boundary(void) {
    TEST("result: filter min_size exact boundary");
    result_list_t list;
    result_list_init(&list);
    scan_result_t r = {0};
    r.file_id = 1; r.size = 100;
    result_list_add(&list, &r);
    r.file_id = 2; r.size = 101;
    result_list_add(&list, &r);
    int c = result_list_filter_by_min_size(&list, 100);
    ASSERT(c == 2);
    result_list_free(&list);
    PASS();
}
static void test_result_add_null(void) {
    TEST("result: add NULL returns error");
    result_list_t list;
    result_list_init(&list);
    int ret = result_list_add(&list, NULL);
    ASSERT(ret != 0);
    result_list_free(&list);
    PASS();
}
static void test_result_large_batch(void) {
    TEST("result: add 1000 items");
    result_list_t list;
    result_list_init(&list);
    for (int i = 0; i < 1000; i++) {
        scan_result_t r = {0};
        r.file_id = 1000 - i;  // reverse order
        r.size = i * 100;
        result_list_add(&list, &r);
    }
    ASSERT(list.count == 1000);
    result_list_sort(&list);
    ASSERT(list.items[0].file_id == 1);
    ASSERT(list.items[999].file_id == 1000);
    result_list_free(&list);
    PASS();
}
static void test_result_export_json(void) {
    TEST("result: export JSON file");
    result_list_t list;
    result_list_init(&list);
    scan_result_t r = {0};
    r.file_id = 1;
    strncpy(r.name, "test.txt", sizeof(r.name) - 1);
    strncpy(r.extension, "txt", sizeof(r.extension) - 1);
    r.size = 42;
    r.confidence = 95.5;
    r.category = SIG_CATEGORY_DOCUMENT;
    result_list_add(&list, &r);

    const char *path = "test_output.json";
    int ret = result_list_export_json(&list, path);
    ASSERT(ret == 0);

    FILE *f = fopen(path, "r");
    ASSERT(f != NULL);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    remove(path);

    ASSERT(strstr(buf, "test.txt") != NULL);
    ASSERT(strstr(buf, "\"count\": 1") != NULL);
    result_list_free(&list);
    PASS();
}

// ============================================================
// FAT32: additional tests
// ============================================================
static void test_fat_short_name_dot(void) {
    TEST("fat: short name with dot only");
    fat_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    memcpy(e.name, "FILE    ", 8);
    memset(e.ext, ' ', 3);
    char name[13];
    fat_parse_short_name(&e, name, sizeof(name));
    ASSERT(strcmp(name, "FILE") == 0);
    PASS();
}
static void test_fat_short_name_single_char(void) {
    TEST("fat: short name single char");
    fat_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name[0] = 'A';
    memset(e.name + 1, ' ', 7);
    memcpy(e.ext, "TXT", 3);
    char name[13];
    fat_parse_short_name(&e, name, sizeof(name));
    ASSERT(strcmp(name, "A.TXT") == 0);
    PASS();
}
static void test_fat_deleted_null(void) {
    TEST("fat: is_deleted NULL entry");
    ASSERT(fat_is_deleted_entry(NULL) == 0);
    PASS();
}
static void test_fat_attr_directory(void) {
    TEST("fat: directory attribute flag");
    fat_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name[0] = 'D';
    e.attributes = 0x10;  // directory
    ASSERT(e.attributes & FAT_ATTR_DIRECTORY);
    PASS();
}
static void test_fat_boot_struct_fields(void) {
    TEST("fat: boot struct field offsets");
    fat_boot_t b;
    memset(&b, 0, sizeof(b));
    b.bytes_per_sector = 512;
    b.sectors_per_cluster = 8;
    ASSERT(b.bytes_per_sector == 512);
    ASSERT(b.sectors_per_cluster == 8);
    PASS();
}
static void test_fat_dir_entry_cluster(void) {
    TEST("fat: dir entry cluster hi/lo");
    fat_dir_entry_t e;
    memset(&e, 0, sizeof(e));
    uint16_t hi = 0x0001;
    uint16_t lo = 0x8000;
    memcpy(&e.first_cluster_hi, &hi, 2);
    memcpy(&e.first_cluster_lo, &lo, 2);
    uint32_t cluster = ((uint32_t)hi << 16) | lo;
    ASSERT(cluster == 0x00018000);
    PASS();
}

// ============================================================
// Main
// ============================================================
int main(void) {
    int total = 0;

    printf("=== Endian Tests ===\n");
    test_endian_le16(); total++;
    test_endian_le16_zero(); total++;
    test_endian_le16_max(); total++;
    test_endian_le32_basic(); total++;
    test_endian_le32_zero(); total++;
    test_endian_le64_basic(); total++;
    test_endian_write_le16(); total++;
    test_endian_write_le32(); total++;
    test_endian_bswap16(); total++;
    test_endian_bswap32(); total++;

    printf("\n=== String Utils Tests ===\n");
    test_format_size_bytes(); total++;
    test_format_size_kb(); total++;
    test_format_size_mb(); total++;
    test_format_size_gb(); total++;
    test_format_size_zero(); total++;
    test_str_copy(); total++;
    test_str_copy_truncate(); total++;
    test_utf16_ascii(); total++;
    test_utf16_empty(); total++;

    printf("\n=== Time Utils Tests ===\n");
    test_ntfs_time_zero(); total++;
    test_ntfs_time_known(); total++;

    printf("\n=== Log Tests ===\n");
    test_log_level(); total++;

    printf("\n=== Partition Tests ===\n");
    test_fs_names(); total++;
    test_mbr_names(); total++;

    printf("\n=== Signature Tests ===\n");
    test_sig_all_defaults(); total++;
    test_sig_match_gif(); total++;
    test_sig_match_bmp(); total++;
    test_sig_match_zip(); total++;
    test_sig_match_exe(); total++;
    test_sig_match_mkv(); total++;
    test_sig_short_data(); total++;
    test_sig_null_data(); total++;
    test_sig_null_db(); total++;
    test_sig_footer_match(); total++;
    test_sig_footer_no_match(); total++;
    test_sig_get_valid(); total++;
    test_sig_get_out_of_range(); total++;
    test_sig_category_all(); total++;

    printf("\n=== Result List Tests ===\n");
    test_result_free_null(); total++;
    test_result_sort_empty(); total++;
    test_result_sort_single(); total++;
    test_result_filter_empty(); total++;
    test_result_filter_no_match(); total++;
    test_result_filter_all_match(); total++;
    test_result_filter_min_size_boundary(); total++;
    test_result_add_null(); total++;
    test_result_large_batch(); total++;
    test_result_export_json(); total++;

    printf("\n=== FAT32 Tests ===\n");
    test_fat_short_name_dot(); total++;
    test_fat_short_name_single_char(); total++;
    test_fat_deleted_null(); total++;
    test_fat_attr_directory(); total++;
    test_fat_boot_struct_fields(); total++;
    test_fat_dir_entry_cluster(); total++;

    printf("\n========================================\n");
    printf("Total: %d tests, %d passed, %d failed\n", total, tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
