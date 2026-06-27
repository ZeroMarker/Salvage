#include "fat.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>

static uint8_t lfn_checksum(const uint8_t *short_name) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i];
    }
    return sum;
}

static void decode_lfn_entry(const fat_lfn_entry_t *lfn, uint16_t *chars, int *count) {
    *count = 0;
    const uint16_t *parts[] = {lfn->name1, lfn->name2, lfn->name3};
    int lens[] = {5, 6, 2};

    for (int p = 0; p < 3; p++) {
        for (int i = 0; i < lens[p]; i++) {
            uint16_t c = read_le16((const uint8_t *)&parts[p][i]);
            if (c == 0x0000 || c == 0xFFFF) return;
            chars[(*count)++] = c;
        }
    }
}

static int utf16_to_utf8_char(uint16_t wc, char *out, int max) {
    if (wc < 0x80) {
        if (max < 1) return 0;
        out[0] = (char)wc;
        return 1;
    } else if (wc < 0x800) {
        if (max < 2) return 0;
        out[0] = (char)(0xC0 | (wc >> 6));
        out[1] = (char)(0x80 | (wc & 0x3F));
        return 2;
    } else {
        if (max < 3) return 0;
        out[0] = (char)(0xE0 | (wc >> 12));
        out[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        out[2] = (char)(0x80 | (wc & 0x3F));
        return 3;
    }
}

static void build_lfn_name(const uint16_t *chars, int count, char *name, int name_size) {
    int pos = 0;
    for (int i = 0; i < count && pos < name_size - 4; i++) {
        int n = utf16_to_utf8_char(chars[i], name + pos, name_size - pos - 1);
        if (n == 0) break;
        pos += n;
    }
    name[pos] = '\0';
}

// Callback type for directory scanning
typedef void (*fat_dir_callback)(const fat_file_info_t *info, void *ctx);

static void process_dir_entry(const fat_dir_entry_t *entry, const char *lfn_name,
                              int is_deleted, fat_dir_callback callback, void *ctx) {
    fat_file_info_t info;
    memset(&info, 0, sizeof(info));

    info.is_deleted = is_deleted;
    info.attributes = entry->attributes;
    info.first_cluster = ((uint32_t)read_le16((const uint8_t *)&entry->first_cluster_hi) << 16) |
                          read_le16((const uint8_t *)&entry->first_cluster_lo);
    info.file_size = read_le32((const uint8_t *)&entry->file_size);
    info.create_date = read_le16((const uint8_t *)&entry->create_date);
    info.create_time = read_le16((const uint8_t *)&entry->create_time);
    info.modify_date = read_le16((const uint8_t *)&entry->modify_date);
    info.modify_time = read_le16((const uint8_t *)&entry->modify_time);
    info.access_date = read_le16((const uint8_t *)&entry->access_date);

    fat_parse_short_name(entry, info.short_name, sizeof(info.short_name));

    if (lfn_name[0] != '\0') {
        strncpy(info.name, lfn_name, sizeof(info.name) - 1);
    } else {
        strncpy(info.name, info.short_name, sizeof(info.name) - 1);
    }

    callback(&info, ctx);
}

static void scan_directory_cluster(fat_volume_t *vol, uint32_t cluster,
                                   fat_dir_callback callback, void *ctx) {
    uint8_t *buf = malloc(vol->cluster_size);
    if (!buf) return;

    int ret = fat_read_cluster(vol, cluster, buf);
    if (ret != SALVAGE_OK) {
        free(buf);
        return;
    }

    int entries_per_cluster = vol->cluster_size / FAT_DIR_ENTRY_SIZE;
    char lfn_name[256];
    uint16_t lfn_chars[256];
    int lfn_count = 0;
    int lfn_valid = 0;

    for (int i = 0; i < entries_per_cluster; i++) {
        const fat_dir_entry_t *entry = (const fat_dir_entry_t *)(buf + i * FAT_DIR_ENTRY_SIZE);

        // End of directory
        if (entry->name[0] == 0x00) break;

        // LFN entry
        if (entry->attributes == FAT_ATTR_LFN) {
            const fat_lfn_entry_t *lfn = (const fat_lfn_entry_t *)entry;
            uint16_t chars[13];
            int count = 0;
            decode_lfn_entry(lfn, chars, &count);

            int order = lfn->order & 0x3F;
            if (lfn->order & 0x40) {
                memset(lfn_chars, 0, sizeof(lfn_chars));
                lfn_valid = 1;
            }

            if (lfn_valid && order > 0) {
                int pos = (order - 1) * 13;
                for (int j = 0; j < count && pos < 255; j++)
                    lfn_chars[pos++] = chars[j];
                if (pos > lfn_count) lfn_count = pos;
            }
            continue;
        }

        // Skip volume label entries
        if (entry->attributes & FAT_ATTR_VOLUME_ID) {
            lfn_valid = 0;
            lfn_count = 0;
            continue;
        }

        // Build LFN
        lfn_name[0] = '\0';
        if (lfn_valid && lfn_count > 0) {
            lfn_chars[lfn_count] = 0;
            build_lfn_name(lfn_chars, lfn_count, lfn_name, sizeof(lfn_name));
        }
        lfn_valid = 0;
        lfn_count = 0;

        // Deleted entry
        if (entry->name[0] == FAT_DELETED_MARKER) {
            process_dir_entry(entry, lfn_name, 1, callback, ctx);
            continue;
        }

        // Normal entry - not deleted
        process_dir_entry(entry, lfn_name, 0, callback, ctx);
    }

    free(buf);
}

// Check if a cluster looks like a valid directory (has . and .. entries)
static int is_valid_directory(fat_volume_t *vol, uint32_t cluster) {
    if (cluster < 2) return 0;
    uint8_t *full = malloc(vol->cluster_size);
    if (!full) return 0;
    if (fat_read_cluster(vol, cluster, full) != SALVAGE_OK) {
        free(full);
        return 0;
    }
    // Check first two entries for . and ..
    int has_dot = (full[0] == '.' && full[0x0B] == FAT_ATTR_DIRECTORY);
    int has_dotdot = (full[32] == '.' && full[33] == '.' && full[32 + 0x0B] == FAT_ATTR_DIRECTORY);
    free(full);
    return has_dot && has_dotdot;
}

// Recursive directory scan - also recurses into non-deleted subdirectories
static void scan_directory(fat_volume_t *vol, uint32_t cluster,
                           fat_dir_callback callback, void *ctx,
                           int max_depth) {
    if (max_depth <= 0) return;

    uint32_t cur = cluster;
    while (cur >= 2 && cur < FAT_CLUSTER_END) {
        uint8_t *buf = malloc(vol->cluster_size);
        if (!buf) break;
        if (fat_read_cluster(vol, cur, buf) != SALVAGE_OK) {
            free(buf);
            break;
        }

        int entries_per_cluster = vol->cluster_size / FAT_DIR_ENTRY_SIZE;
        char lfn_name[256];
        uint16_t lfn_chars[256];
        int lfn_count = 0;
        int lfn_valid = 0;

        for (int i = 0; i < entries_per_cluster; i++) {
            const fat_dir_entry_t *entry = (const fat_dir_entry_t *)(buf + i * FAT_DIR_ENTRY_SIZE);
            if (entry->name[0] == 0x00) break;

            if (entry->attributes == FAT_ATTR_LFN) {
                const fat_lfn_entry_t *lfn = (const fat_lfn_entry_t *)entry;
                uint16_t chars[13];
                int count = 0;
                decode_lfn_entry(lfn, chars, &count);
                int order = lfn->order & 0x3F;
                if (lfn->order & 0x40) {
                    memset(lfn_chars, 0, sizeof(lfn_chars));
                    lfn_valid = 1;
                }
                if (lfn_valid && order > 0) {
                    int pos = (order - 1) * 13;
                    for (int j = 0; j < count && pos < 255; j++)
                        lfn_chars[pos++] = chars[j];
                    if (pos > lfn_count) lfn_count = pos;
                }
                continue;
            }

            if (entry->attributes & FAT_ATTR_VOLUME_ID) {
                lfn_valid = 0; lfn_count = 0;
                continue;
            }

            lfn_name[0] = '\0';
            if (lfn_valid && lfn_count > 0) {
                lfn_chars[lfn_count] = 0;
                build_lfn_name(lfn_chars, lfn_count, lfn_name, sizeof(lfn_name));
            }
            lfn_valid = 0;
            lfn_count = 0;

            if (entry->name[0] == FAT_DELETED_MARKER) {
                // Deleted entry - report it
                process_dir_entry(entry, lfn_name, 1, callback, ctx);

                // Also check if this was a directory and scan its clusters
                if (entry->attributes & FAT_ATTR_DIRECTORY) {
                    uint32_t sub_cluster = ((uint32_t)read_le16((const uint8_t *)&entry->first_cluster_hi) << 16) |
                                            read_le16((const uint8_t *)&entry->first_cluster_lo);
                    if (sub_cluster >= 2 && is_valid_directory(vol, sub_cluster)) {
                        scan_directory(vol, sub_cluster, callback, ctx, max_depth - 1);
                    }
                }
            } else {
                process_dir_entry(entry, lfn_name, 0, callback, ctx);

                // Recurse into non-deleted directories (skip . and ..)
                if ((entry->attributes & FAT_ATTR_DIRECTORY) && entry->name[0] != '.') {
                    uint32_t sub_cluster = ((uint32_t)read_le16((const uint8_t *)&entry->first_cluster_hi) << 16) |
                                            read_le16((const uint8_t *)&entry->first_cluster_lo);
                    if (sub_cluster >= 2) {
                        scan_directory(vol, sub_cluster, callback, ctx, max_depth - 1);
                    }
                }
            }
        }

        free(buf);
        cur = fat_next_cluster(vol, cur);
    }
}

// Internal scan context and callback for scanner integration
typedef struct {
    fat_volume_t *vol;
    void (*callback)(fat_file_info_t *info, void *ctx);
    void *ctx;
} fat_scan_ctx_t;

static void deleted_file_callback(const fat_file_info_t *info, void *ctx) {
    if (!info->is_deleted) return;
    fat_scan_ctx_t *sctx = (fat_scan_ctx_t *)ctx;
    sctx->callback((fat_file_info_t *)info, sctx->ctx);
}

int fat_scan_deleted(fat_volume_t *vol,
                     void (*callback)(fat_file_info_t *info, void *ctx),
                     void *ctx) {
    if (!vol || !callback) return SALVAGE_ERR_INVALID;

    fat_scan_ctx_t sctx = {vol, callback, ctx};
    scan_directory(vol, vol->root_cluster, deleted_file_callback, &sctx, 10);
    return SALVAGE_OK;
}
