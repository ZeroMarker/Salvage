#include "exfat.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>

int exfat_init(exfat_volume_t *vol, device_t *dev, uint64_t partition_start) {
    if (!vol || !dev) return SALVAGE_ERR_INVALID;

    memset(vol, 0, sizeof(exfat_volume_t));
    vol->device = dev;
    vol->partition_start = partition_start;

    uint8_t sector[512];
    int ret = device_read_sectors(dev, partition_start, 1, sector);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read exFAT boot sector");
        return ret;
    }

    exfat_boot_t *boot = (exfat_boot_t *)sector;

    // Validate jump
    if (boot->jump[0] != 0xEB && boot->jump[0] != 0xE9) {
        return SALVAGE_ERR_NOT_FOUND;
    }

    // Validate signature
    if (boot->end_signature != 0x55AA) {
        return SALVAGE_ERR_NOT_FOUND;
    }

    // Validate FS name
    if (memcmp(boot->fs_name, "EXFAT   ", 8) != 0) {
        LOG_DEBUG("Not exFAT filesystem");
        return SALVAGE_ERR_NOT_FOUND;
    }

    vol->bytes_per_sector = 1 << boot->bytes_per_sector_shift;
    vol->sectors_per_cluster = 1 << boot->sectors_per_cluster_shift;
    vol->cluster_size = vol->bytes_per_sector * vol->sectors_per_cluster;
    vol->fat_offset = read_le32((const uint8_t *)&boot->fat_offset);
    vol->fat_length = read_le32((const uint8_t *)&boot->fat_length);
    vol->cluster_heap_offset = read_le32((const uint8_t *)&boot->cluster_heap_offset);
    vol->cluster_count = read_le32((const uint8_t *)&boot->cluster_count);
    vol->root_dir_cluster = read_le32((const uint8_t *)&boot->first_cluster);
    vol->num_fats = boot->num_fats;
    vol->serial_number = read_le32((const uint8_t *)&boot->volume_serial);

    LOG_INFO("exFAT: cluster_size=%u, clusters=%u, root_cluster=%u",
             vol->cluster_size, vol->cluster_count, vol->root_dir_cluster);

    return SALVAGE_OK;
}

int exfat_read_cluster(exfat_volume_t *vol, uint32_t cluster, void *buf) {
    if (!vol || !buf || cluster < 2) return SALVAGE_ERR_INVALID;

    uint64_t lba = vol->partition_start + vol->cluster_heap_offset +
                   (uint64_t)(cluster - 2) * vol->sectors_per_cluster;
    return device_read_sectors(vol->device, lba, vol->sectors_per_cluster, buf);
}

uint32_t exfat_next_cluster(exfat_volume_t *vol, uint32_t cluster) {
    if (!vol || cluster < 2) return EXFAT_CLUSTER_END;

    uint32_t fat_offset = cluster * 4;
    uint64_t fat_sector = vol->partition_start + vol->fat_offset +
                          (fat_offset / vol->bytes_per_sector);
    uint32_t entry_offset = fat_offset % vol->bytes_per_sector;

    uint8_t sector[512];
    if (device_read_sectors(vol->device, fat_sector, 1, sector) != SALVAGE_OK) {
        return EXFAT_CLUSTER_END;
    }

    return read_le32(sector + entry_offset);
}
