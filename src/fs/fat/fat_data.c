#include "fat.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>

int fat_read_file_data(fat_volume_t *vol, const fat_file_info_t *info,
                       uint64_t offset, uint64_t size, void *buf) {
    if (!vol || !info || !buf) return SALVAGE_ERR_INVALID;
    if (info->first_cluster < 2) return SALVAGE_ERR_INVALID;

    uint8_t *out = (uint8_t *)buf;
    uint64_t bytes_remaining = size;
    uint32_t cluster_size = vol->cluster_size;
    uint32_t cluster = info->first_cluster;

    // Skip clusters to reach offset
    while (offset >= cluster_size) {
        cluster = fat_next_cluster(vol, cluster);
        if (cluster >= FAT_CLUSTER_END) return SALVAGE_ERR_IO;
        offset -= cluster_size;
    }

    uint8_t *tmp = malloc(cluster_size);
    if (!tmp) return SALVAGE_ERR_NO_MEMORY;

    while (bytes_remaining > 0 && cluster >= 2 && cluster < FAT_CLUSTER_END) {
        int ret = fat_read_cluster(vol, cluster, tmp);
        if (ret != SALVAGE_OK) {
            free(tmp);
            return ret;
        }

        uint64_t copy_start = offset;
        uint64_t copy_size = cluster_size - copy_start;
        if (copy_size > bytes_remaining) copy_size = bytes_remaining;

        memcpy(out, tmp + copy_start, copy_size);
        out += copy_size;
        bytes_remaining -= copy_size;
        offset = 0;

        cluster = fat_next_cluster(vol, cluster);
    }

    if (bytes_remaining > 0) {
        memset(out, 0, bytes_remaining);
    }

    free(tmp);
    return SALVAGE_OK;
}
