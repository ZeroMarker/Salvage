#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core/result.h"
#include "signature/signature.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_result_list_init(void) {
    TEST("result_list_init");
    result_list_t list;
    result_list_init(&list);
    if (list.items == NULL && list.count == 0 && list.capacity == 0) PASS(); else FAIL();
}

static void test_result_list_add(void) {
    TEST("result_list_add");
    result_list_t list;
    result_list_init(&list);
    
    scan_result_t r;
    memset(&r, 0, sizeof(r));
    r.file_id = 1;
    strncpy(r.name, "test.jpg", sizeof(r.name) - 1);
    r.size = 1024;
    r.confidence = 95.0;
    
    int ret = result_list_add(&list, &r);
    if (ret == 0 && list.count == 1 && list.items[0].file_id == 1) PASS(); else FAIL();
    
    result_list_free(&list);
}

static void test_result_list_add_multiple(void) {
    TEST("result_list_add_multiple");
    result_list_t list;
    result_list_init(&list);
    
    for (int i = 0; i < 100; i++) {
        scan_result_t r;
        memset(&r, 0, sizeof(r));
        r.file_id = i;
        result_list_add(&list, &r);
    }
    
    if (list.count == 100 && list.capacity >= 100) PASS(); else FAIL();
    
    result_list_free(&list);
}

static void test_result_list_sort(void) {
    TEST("result_list_sort");
    result_list_t list;
    result_list_init(&list);
    
    uint64_t ids[] = {5, 2, 8, 1, 9, 3};
    for (int i = 0; i < 6; i++) {
        scan_result_t r;
        memset(&r, 0, sizeof(r));
        r.file_id = ids[i];
        result_list_add(&list, &r);
    }
    
    result_list_sort(&list);
    
    int sorted = 1;
    for (int i = 1; i < list.count; i++) {
        if (list.items[i].file_id < list.items[i-1].file_id) {
            sorted = 0;
            break;
        }
    }
    
    if (sorted && list.count == 6) PASS(); else FAIL();
    
    result_list_free(&list);
}

static void test_result_filter_category(void) {
    TEST("result_list_filter_by_category");
    result_list_t list;
    result_list_init(&list);
    
    scan_result_t r;
    memset(&r, 0, sizeof(r));
    
    r.file_id = 1; r.category = SIG_CATEGORY_IMAGE;
    result_list_add(&list, &r);
    r.file_id = 2; r.category = SIG_CATEGORY_DOCUMENT;
    result_list_add(&list, &r);
    r.file_id = 3; r.category = SIG_CATEGORY_IMAGE;
    result_list_add(&list, &r);
    r.file_id = 4; r.category = SIG_CATEGORY_AUDIO;
    result_list_add(&list, &r);
    
    int count = result_list_filter_by_category(&list, SIG_CATEGORY_IMAGE);
    
    if (count == 2 && list.count == 2 &&
        list.items[0].file_id == 1 && list.items[1].file_id == 3) PASS(); else FAIL();
    
    result_list_free(&list);
}

static void test_result_filter_min_size(void) {
    TEST("result_list_filter_by_min_size");
    result_list_t list;
    result_list_init(&list);
    
    scan_result_t r;
    memset(&r, 0, sizeof(r));
    
    r.file_id = 1; r.size = 100;
    result_list_add(&list, &r);
    r.file_id = 2; r.size = 500;
    result_list_add(&list, &r);
    r.file_id = 3; r.size = 1000;
    result_list_add(&list, &r);
    r.file_id = 4; r.size = 50;
    result_list_add(&list, &r);
    
    int count = result_list_filter_by_min_size(&list, 200);
    
    if (count == 2 && list.count == 2 &&
        list.items[0].file_id == 2 && list.items[1].file_id == 3) PASS(); else FAIL();
    
    result_list_free(&list);
}

static void test_sig_load_defaults(void) {
    TEST("sig_load_defaults");
    signature_db_t db;
    int ret = sig_load_defaults(&db);
    
    if (ret == 0 && db.count > 0) {
        // Check that WAV, AVI, WebP all exist
        int found_wav = 0, found_avi = 0, found_webp = 0;
        for (int i = 0; i < db.count; i++) {
            if (strcmp(db.entries[i].name, "WAV") == 0) found_wav = 1;
            if (strcmp(db.entries[i].name, "AVI") == 0) found_avi = 1;
            if (strcmp(db.entries[i].name, "WebP") == 0) found_webp = 1;
        }
        if (found_wav && found_avi && found_webp) PASS(); else FAIL();
    } else {
        FAIL();
    }
}

static void test_sig_match_riff_wav(void) {
    TEST("sig_match_header RIFF->WAV");
    signature_db_t db;
    sig_load_defaults(&db);
    
    // RIFF header + WAVE at offset 8
    uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45};
    int idx = sig_match_header(&db, data, sizeof(data));
    
    if (idx >= 0 && strcmp(db.entries[idx].name, "WAV") == 0) PASS(); else FAIL();
}

static void test_sig_match_riff_avi(void) {
    TEST("sig_match_header RIFF->AVI");
    signature_db_t db;
    sig_load_defaults(&db);
    
    // RIFF header + AVI  at offset 8
    uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x41, 0x56, 0x49, 0x20};
    int idx = sig_match_header(&db, data, sizeof(data));
    
    if (idx >= 0 && strcmp(db.entries[idx].name, "AVI") == 0) PASS(); else FAIL();
}

static void test_sig_match_riff_webp(void) {
    TEST("sig_match_header RIFF->WebP");
    signature_db_t db;
    sig_load_defaults(&db);
    
    // RIFF header + WEBP at offset 8
    uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50};
    int idx = sig_match_header(&db, data, sizeof(data));
    
    if (idx >= 0 && strcmp(db.entries[idx].name, "WebP") == 0) PASS(); else FAIL();
}

static void test_sig_match_jpeg(void) {
    TEST("sig_match_header JPEG");
    signature_db_t db;
    sig_load_defaults(&db);
    
    uint8_t data[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    int idx = sig_match_header(&db, data, sizeof(data));
    
    if (idx >= 0 && strcmp(db.entries[idx].name, "JPEG") == 0) PASS(); else FAIL();
}

static void test_sig_category_name(void) {
    TEST("sig_category_name");
    if (strcmp(sig_category_name(SIG_CATEGORY_IMAGE), "Image") == 0 &&
        strcmp(sig_category_name(SIG_CATEGORY_VIDEO), "Video") == 0 &&
        strcmp(sig_category_name(SIG_CATEGORY_UNKNOWN), "Unknown") == 0) PASS(); else FAIL();
}

int main(void) {
    printf("Scanner Tests:\n");
    
    test_result_list_init();
    test_result_list_add();
    test_result_list_add_multiple();
    test_result_list_sort();
    test_result_filter_category();
    test_result_filter_min_size();
    
    printf("\nSignature Tests:\n");
    
    test_sig_load_defaults();
    test_sig_match_riff_wav();
    test_sig_match_riff_avi();
    test_sig_match_riff_webp();
    test_sig_match_jpeg();
    test_sig_category_name();
    
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
