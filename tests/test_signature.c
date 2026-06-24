#include <stdio.h>
#include <string.h>
#include "signature/signature.h"
#include "utils/log.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL() do { printf("FAIL\n"); tests_failed++; } while(0)

static void test_load_defaults(void) {
    TEST("sig_load_defaults");
    signature_db_t db;
    int ret = sig_load_defaults(&db);
    if (ret == SALVAGE_OK && db.count > 0) PASS(); else FAIL();
}

static void test_match_jpeg(void) {
    TEST("sig_match_header (JPEG)");
    signature_db_t db;
    sig_load_defaults(&db);
    
    uint8_t jpeg_header[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    int idx = sig_match_header(&db, jpeg_header, sizeof(jpeg_header));
    
    if (idx >= 0 && strcmp(sig_get(&db, idx)->name, "JPEG") == 0) PASS(); else FAIL();
}

static void test_match_png(void) {
    TEST("sig_match_header (PNG)");
    signature_db_t db;
    sig_load_defaults(&db);
    
    uint8_t png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    int idx = sig_match_header(&db, png_header, sizeof(png_header));
    
    if (idx >= 0 && strcmp(sig_get(&db, idx)->name, "PNG") == 0) PASS(); else FAIL();
}

static void test_match_pdf(void) {
    TEST("sig_match_header (PDF)");
    signature_db_t db;
    sig_load_defaults(&db);
    
    uint8_t pdf_header[] = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31};
    int idx = sig_match_header(&db, pdf_header, sizeof(pdf_header));
    
    if (idx >= 0 && strcmp(sig_get(&db, idx)->name, "PDF") == 0) PASS(); else FAIL();
}

static void test_no_match(void) {
    TEST("sig_match_header (no match)");
    signature_db_t db;
    sig_load_defaults(&db);
    
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    int idx = sig_match_header(&db, data, sizeof(data));
    
    if (idx < 0) PASS(); else FAIL();
}

static void test_category_name(void) {
    TEST("sig_category_name");
    if (strcmp(sig_category_name(SIG_CATEGORY_IMAGE), "Image") == 0 &&
        strcmp(sig_category_name(SIG_CATEGORY_DOCUMENT), "Document") == 0) {
        PASS();
    } else {
        FAIL();
    }
}

int main(void) {
    printf("Signature Tests:\n");
    
    test_load_defaults();
    test_match_jpeg();
    test_match_png();
    test_match_pdf();
    test_no_match();
    test_category_name();
    
    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
