#ifndef SALVAGE_SIGNATURE_H
#define SALVAGE_SIGNATURE_H

#include <salvage/salvage.h>
#include <stdint.h>

// File category
typedef enum {
    SIG_CATEGORY_UNKNOWN = 0,
    SIG_CATEGORY_IMAGE,
    SIG_CATEGORY_DOCUMENT,
    SIG_CATEGORY_AUDIO,
    SIG_CATEGORY_VIDEO,
    SIG_CATEGORY_ARCHIVE,
    SIG_CATEGORY_EXECUTABLE,
    SIG_CATEGORY_DATABASE,
} sig_category_t;

// File signature entry
typedef struct {
    char name[32];           // e.g., "JPEG"
    char extensions[64];     // e.g., "jpg,jpeg"
    uint8_t header[32];      // Header bytes
    int header_len;          // Header length
    uint8_t footer[16];      // Footer bytes
    int footer_len;          // Footer length
    uint64_t max_size;       // Maximum expected file size
    sig_category_t category;
} signature_t;

// Signature database
typedef struct {
    signature_t entries[256];
    int count;
} signature_db_t;

// Load signature database from JSON file
int sig_load(signature_db_t *db, const char *path);

// Load default built-in signatures
int sig_load_defaults(signature_db_t *db);

// Match header bytes against database
// Returns index of matching signature or -1
int sig_match_header(const signature_db_t *db, const uint8_t *data, int len);

// Match footer bytes for a specific signature
int sig_match_footer(const signature_db_t *db, int sig_index,
                     const uint8_t *data, int len);

// Get signature by index
const signature_t* sig_get(const signature_db_t *db, int index);

// Get category name
const char* sig_category_name(sig_category_t cat);

#endif // SALVAGE_SIGNATURE_H
