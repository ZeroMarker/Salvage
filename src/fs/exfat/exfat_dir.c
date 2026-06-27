#include "exfat.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>

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

static void decode_name_entries(const uint16_t *chars, int count,
                                char *name, int name_size) {
    int pos = 0;
    for (int i = 0; i < count && pos < name_size - 4; i++) {
        int n = utf16_to_utf8_char(chars[i], name + pos, name_size - pos - 1);
        if (n == 0) break;
        pos += n;
    }
    name[pos] = '\0';
}

typedef void (*exfat_dir_callback)(const exfat_file_info_t *info, void *ctx);

static void scan_directory_cluster(exfat_volume_t *vol, uint32_t cluster,
                                   exfat_dir_callback callback, void *ctx) {
    uint8_t *buf = malloc(vol->cluster_size);
    if (!buf) return;

    int ret = exfat_read_cluster(vol, cluster, buf);
    if (ret != SALVAGE_OK) {
        free(buf);
        return;
    }

    int entries_per_cluster = vol->cluster_size / 32;

    for (int i = 0; i < entries_per_cluster; i++) {
        uint8_t *entry = buf + i * 32;
        uint8_t entry_type = entry[0];

        // End of directory
        if (entry_type == 0x00) break;

        // File Directory Entry (in-use 0x85 or deleted 0x05)
        if (entry_type == EXFAT_ENTRY_FILE || entry_type == EXFAT_DELETED_MARKER) {
            exfat_file_entry_t *file_entry = (exfat_file_entry_t *)entry;
            int is_deleted = (entry_type == EXFAT_DELETED_MARKER);

            // Read secondary entries
            int secondary_count = file_entry->secondary_count;
            if (i + secondary_count >= entries_per_cluster) break;

            exfat_file_info_t info;
            memset(&info, 0, sizeof(info));
            info.is_deleted = is_deleted;
            info.attributes = read_le16((const uint8_t *)&file_entry->attributes);
            info.create_time = read_le32((const uint8_t *)&file_entry->create_time);
            info.modify_time = read_le32((const uint8_t *)&file_entry->modify_time);
            info.access_time = read_le32((const uint8_t *)&file_entry->access_time);

            uint16_t name_chars[256];
            int name_len = 0;

            for (int j = 1; j <= secondary_count && (i + j) < entries_per_cluster; j++) {
                uint8_t *sub = buf + (i + j) * 32;
                uint8_t sub_type = sub[0];

                // Stream Extension (0xC0 or 0x40 for deleted)
                if (sub_type == EXFAT_ENTRY_STREAM || sub_type == 0x40) {
                    exfat_stream_entry_t *stream = (exfat_stream_entry_t *)sub;
                    info.first_cluster = read_le32((const uint8_t *)&stream->first_cluster);
                    info.data_length = read_le64((const uint8_t *)&stream->data_length);
                    info.valid_data_length = read_le64((const uint8_t *)&stream->valid_data_length);
                    name_len = stream->name_length;
                }

                // File Name (0xC1 or 0x41 for deleted)
                if (sub_type == EXFAT_ENTRY_NAME || sub_type == 0x41) {
                    exfat_name_entry_t *name_entry = (exfat_name_entry_t *)sub;
                    for (int k = 0; k < 15; k++) {
                        uint16_t c = read_le16((const uint8_t *)&name_entry->name[k]);
                        if (c == 0x0000) break;
                        if (name_len < 255) name_chars[name_len++] = c;
                    }
                }
            }

            if (name_len > 0) {
                name_chars[name_len] = 0;
                decode_name_entries(name_chars, name_len, info.name, sizeof(info.name));
            }

            callback(&info, ctx);
            i += secondary_count;
        }
    }

    free(buf);
}

static void scan_directory(exfat_volume_t *vol, uint32_t cluster,
                           exfat_dir_callback callback, void *ctx) {
    uint32_t cur = cluster;
    while (cur >= 2 && cur < EXFAT_CLUSTER_END) {
        scan_directory_cluster(vol, cur, callback, ctx);
        cur = exfat_next_cluster(vol, cur);
    }
}

typedef struct {
    exfat_volume_t *vol;
    void (*callback)(exfat_file_info_t *info, void *ctx);
    void *ctx;
} exfat_scan_ctx_t;

static void deleted_file_callback(const exfat_file_info_t *info, void *ctx) {
    if (!info->is_deleted) return;
    exfat_scan_ctx_t *sctx = (exfat_scan_ctx_t *)ctx;
    sctx->callback((exfat_file_info_t *)info, sctx->ctx);
}

int exfat_scan_deleted(exfat_volume_t *vol,
                       void (*callback)(exfat_file_info_t *info, void *ctx),
                       void *ctx) {
    if (!vol || !callback) return SALVAGE_ERR_INVALID;

    exfat_scan_ctx_t sctx = {vol, callback, ctx};
    scan_directory(vol, vol->root_dir_cluster, deleted_file_callback, &sctx);
    return SALVAGE_OK;
}
