#include "signature.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Parse hex string to bytes
static int parse_hex(const char *hex, uint8_t *out, int max_len) {
    int len = 0;
    while (*hex && len < max_len) {
        // Skip spaces
        while (*hex == ' ') hex++;
        if (!*hex) break;
        
        // Parse hex byte
        char buf[3] = {hex[0], hex[1], 0};
        if (buf[1] == 0) break;  // Need two chars
        
        out[len++] = (uint8_t)strtol(buf, NULL, 16);
        hex += 2;
    }
    return len;
}

// Parse category string
static sig_category_t parse_category(const char *cat) {
    if (strcmp(cat, "image") == 0) return SIG_CATEGORY_IMAGE;
    if (strcmp(cat, "document") == 0) return SIG_CATEGORY_DOCUMENT;
    if (strcmp(cat, "audio") == 0) return SIG_CATEGORY_AUDIO;
    if (strcmp(cat, "video") == 0) return SIG_CATEGORY_VIDEO;
    if (strcmp(cat, "archive") == 0) return SIG_CATEGORY_ARCHIVE;
    if (strcmp(cat, "executable") == 0) return SIG_CATEGORY_EXECUTABLE;
    if (strcmp(cat, "database") == 0) return SIG_CATEGORY_DATABASE;
    return SIG_CATEGORY_UNKNOWN;
}

int sig_load(signature_db_t *db, const char *path) {
    if (!db || !path) return SALVAGE_ERR_INVALID;
    
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("Cannot open signature file: %s", path);
        return SALVAGE_ERR_IO;
    }
    
    // Read entire file
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *json = malloc(file_size + 1);
    if (!json) {
        fclose(f);
        return SALVAGE_ERR_NO_MEMORY;
    }
    
    fread(json, 1, file_size, f);
    json[file_size] = '\0';
    fclose(f);
    
    // Simple JSON parser (no external dependency)
    memset(db, 0, sizeof(signature_db_t));
    
    // Find "signatures" array
    char *p = strstr(json, "\"signatures\"");
    if (!p) {
        free(json);
        return SALVAGE_ERR_INVALID;
    }
    
    p = strchr(p, '[');
    if (!p) { free(json); return SALVAGE_ERR_INVALID; }
    p++;
    
    // Parse each signature object
    while (*p && db->count < 256) {
        // Find next object
        p = strchr(p, '{');
        if (!p) break;
        p++;
        
        signature_t *sig = &db->entries[db->count];
        memset(sig, 0, sizeof(signature_t));
        
        // Parse fields
        while (*p && *p != '}') {
            // Find key
            char *key_start = strchr(p, '"');
            if (!key_start || key_start > strchr(p, '}')) break;
            key_start++;
            
            char *key_end = strchr(key_start, '"');
            if (!key_end) break;
            
            char key[32] = {0};
            int key_len = key_end - key_start;
            if (key_len >= 32) key_len = 31;
            strncpy(key, key_start, key_len);
            
            // Find value
            char *val_start = strchr(key_end + 1, ':');
            if (!val_start) break;
            val_start++;
            while (*val_start == ' ') val_start++;
            
            if (*val_start == '"') {
                // String value
                val_start++;
                char *val_end = strchr(val_start, '"');
                if (!val_end) break;
                
                char val[128] = {0};
                int val_len = val_end - val_start;
                if (val_len >= 128) val_len = 127;
                strncpy(val, val_start, val_len);
                
                if (strcmp(key, "name") == 0) {
                    strncpy(sig->name, val, sizeof(sig->name) - 1);
                } else if (strcmp(key, "extensions") == 0) {
                    strncpy(sig->extensions, val, sizeof(sig->extensions) - 1);
                } else if (strcmp(key, "header") == 0) {
                    sig->header_len = parse_hex(val, sig->header, 32);
                } else if (strcmp(key, "footer") == 0) {
                    sig->footer_len = parse_hex(val, sig->footer, 16);
                } else if (strcmp(key, "category") == 0) {
                    sig->category = parse_category(val);
                }
                
                p = val_end + 1;
            } else {
                // Numeric value
                char *val_end = val_start;
                while (*val_end && *val_end != ',' && *val_end != '}' && *val_end != '\n') val_end++;
                
                char val[32] = {0};
                int val_len = val_end - val_start;
                if (val_len >= 32) val_len = 31;
                strncpy(val, val_start, val_len);
                
                if (strcmp(key, "maxSize") == 0) {
                    sig->max_size = strtoull(val, NULL, 10);
                }
                
                p = val_end;
            }
        }
        
        // Skip to end of object
        p = strchr(p, '}');
        if (p) p++;
        
        // Validate entry
        if (sig->name[0] && sig->header_len > 0) {
            db->count++;
            LOG_DEBUG("Loaded signature: %s (%d header bytes)", sig->name, sig->header_len);
        }
    }
    
    free(json);
    LOG_INFO("Loaded %d signatures from %s", db->count, path);
    return SALVAGE_OK;
}

int sig_load_defaults(signature_db_t *db) {
    if (!db) return SALVAGE_ERR_INVALID;
    
    memset(db, 0, sizeof(signature_db_t));
    
    // Image formats
    struct { const char *name; const char *ext; const char *hdr; const char *ftr; sig_category_t cat; uint64_t max; } defaults[] = {
        {"JPEG", "jpg,jpeg", "FF D8 FF", "FF D9", SIG_CATEGORY_IMAGE, 52428800},
        {"PNG", "png", "89 50 4E 47 0D 0A 1A 0A", "49 45 4E 44 AE 42 60 82", SIG_CATEGORY_IMAGE, 104857600},
        {"GIF", "gif", "47 49 46 38", "3B", SIG_CATEGORY_IMAGE, 52428800},
        {"BMP", "bmp", "42 4D", "", SIG_CATEGORY_IMAGE, 104857600},
        {"PDF", "pdf", "25 50 44 46", "25 25 45 4F 46", SIG_CATEGORY_DOCUMENT, 104857600},
        {"DOC", "doc,xls,ppt", "D0 CF 11 E0 A1 B1 1A E1", "", SIG_CATEGORY_DOCUMENT, 104857600},
        {"ZIP", "zip", "50 4B 03 04", "50 4B 05 06", SIG_CATEGORY_ARCHIVE, 4294967296},
        {"RAR", "rar", "52 61 72 21 1A 07", "", SIG_CATEGORY_ARCHIVE, 4294967296},
        {"MP3", "mp3", "49 44 33", "", SIG_CATEGORY_AUDIO, 52428800},
        {"WAV", "wav", "52 49 46 46", "", SIG_CATEGORY_AUDIO, 524288000},
        {"AVI", "avi", "52 49 46 46", "", SIG_CATEGORY_VIDEO, 4294967296},
        {"MKV", "mkv", "1A 45 DF A3", "", SIG_CATEGORY_VIDEO, 4294967296},
        {"EXE", "exe,dll", "4D 5A", "", SIG_CATEGORY_EXECUTABLE, 524288000},
        {NULL, NULL, NULL, NULL, 0, 0}
    };
    
    for (int i = 0; defaults[i].name; i++) {
        signature_t *sig = &db->entries[db->count];
        strncpy(sig->name, defaults[i].name, sizeof(sig->name) - 1);
        strncpy(sig->extensions, defaults[i].ext, sizeof(sig->extensions) - 1);
        sig->header_len = parse_hex(defaults[i].hdr, sig->header, 32);
        sig->footer_len = parse_hex(defaults[i].ftr, sig->footer, 16);
        sig->category = defaults[i].cat;
        sig->max_size = defaults[i].max;
        db->count++;
    }
    
    LOG_INFO("Loaded %d default signatures", db->count);
    return SALVAGE_OK;
}

int sig_match_header(const signature_db_t *db, const uint8_t *data, int len) {
    if (!db || !data || len < 4) return -1;
    
    for (int i = 0; i < db->count; i++) {
        const signature_t *sig = &db->entries[i];
        if (sig->header_len == 0) continue;
        if (sig->header_len > len) continue;
        
        if (memcmp(data, sig->header, sig->header_len) == 0) {
            return i;
        }
    }
    
    return -1;
}

int sig_match_footer(const signature_db_t *db, int sig_index,
                     const uint8_t *data, int len) {
    if (!db || !data || sig_index < 0 || sig_index >= db->count) return 0;
    
    const signature_t *sig = &db->entries[sig_index];
    if (sig->footer_len == 0) return 0;  // No footer to match
    if (len < sig->footer_len) return 0;
    
    // Check at end of data
    return memcmp(data + len - sig->footer_len, sig->footer, sig->footer_len) == 0;
}

const signature_t* sig_get(const signature_db_t *db, int index) {
    if (!db || index < 0 || index >= db->count) return NULL;
    return &db->entries[index];
}

const char* sig_category_name(sig_category_t cat) {
    switch (cat) {
        case SIG_CATEGORY_IMAGE:      return "Image";
        case SIG_CATEGORY_DOCUMENT:   return "Document";
        case SIG_CATEGORY_AUDIO:      return "Audio";
        case SIG_CATEGORY_VIDEO:      return "Video";
        case SIG_CATEGORY_ARCHIVE:    return "Archive";
        case SIG_CATEGORY_EXECUTABLE: return "Executable";
        case SIG_CATEGORY_DATABASE:   return "Database";
        default:                      return "Unknown";
    }
}
