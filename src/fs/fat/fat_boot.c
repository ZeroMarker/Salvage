#include "fat.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>

int fat_init(fat_volume_t *vol, device_t *dev, uint64_t partition_start) {
    if (!vol || !dev) return SALVAGE_ERR_INVALID;

    memset(vol, 0, sizeof(fat_volume_t));
    vol->device = dev;
    vol->partition_start = partition_start;

    uint8_t sector[512];
    int ret = device_read_sectors(dev, partition_start, 1, sector);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read FAT boot sector");
        return ret;
    }

    fat_boot_t *boot = (fat_boot_t *)sector;

    // Validate jump instruction
    if (boot->jump[0] != 0xEB && boot->jump[0] != 0xE9) {
        LOG_DEBUG("Invalid FAT jump instruction: 0x%02X", boot->jump[0]);
        return SALVAGE_ERR_NOT_FOUND;
    }

    // Validate boot signature
    if (boot->end_signature != 0x55AA) {
        LOG_DEBUG("Invalid FAT boot signature: 0x%04X", boot->end_signature);
        return SALVAGE_ERR_NOT_FOUND;
    }

    // Validate FAT32
    if (boot->fat_size_16 != 0 || boot->root_entry_count != 0) {
        LOG_DEBUG("Not FAT32 (fat_size_16=%u, root_entries=%u)",
                  boot->fat_size_16, boot->root_entry_count);
        return SALVAGE_ERR_NOT_FOUND;
    }

    vol->bytes_per_sector = read_le16((const uint8_t *)&boot->bytes_per_sector);
    vol->sectors_per_cluster = boot->sectors_per_cluster;
    vol->cluster_size = vol->bytes_per_sector * vol->sectors_per_cluster;
    vol->reserved_sectors = read_le16((const uint8_t *)&boot->reserved_sectors);
    vol->num_fats = boot->num_fats;
    vol->fat_size_sectors = read_le32((const uint8_t *)&boot->fat_size_32);
    vol->root_cluster = read_le32((const uint8_t *)&boot->root_cluster);
    vol->serial_number = read_le32((const uint8_t *)&boot->volume_serial);

    vol->fat_start_lba = (uint32_t)(partition_start + vol->reserved_sectors);
    vol->data_start_lba = (uint32_t)(partition_start + vol->reserved_sectors +
                                     vol->num_fats * vol->fat_size_sectors);

    uint32_t total_sectors = read_le32((const uint8_t *)&boot->total_sectors_32);
    uint32_t data_sectors = total_sectors - vol->reserved_sectors -
                            vol->num_fats * vol->fat_size_sectors;
    vol->total_clusters = data_sectors / vol->sectors_per_cluster;

    LOG_INFO("FAT32: cluster_size=%u, total_clusters=%u, root_cluster=%u",
             vol->cluster_size, vol->total_clusters, vol->root_cluster);

    return SALVAGE_OK;
}

int fat_read_cluster(fat_volume_t *vol, uint32_t cluster, void *buf) {
    if (!vol || !buf || cluster < 2) return SALVAGE_ERR_INVALID;

    uint64_t lba = vol->data_start_lba +
                   (uint64_t)(cluster - 2) * vol->sectors_per_cluster;
    return device_read_sectors(vol->device, lba, vol->sectors_per_cluster, buf);
}

uint32_t fat_next_cluster(fat_volume_t *vol, uint32_t cluster) {
    if (!vol || cluster < 2) return FAT_CLUSTER_END;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t entry_offset = fat_offset % vol->bytes_per_sector;

    uint8_t sector[512];
    if (device_read_sectors(vol->device, fat_sector, 1, sector) != SALVAGE_OK) {
        return FAT_CLUSTER_END;
    }

    uint32_t next = read_le32(sector + entry_offset) & 0x0FFFFFFF;
    return next;
}

void fat_parse_short_name(const fat_dir_entry_t *entry, char *name, int name_size) {
    if (!entry || !name || name_size < 13) return;

    int pos = 0;
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        if (pos < name_size - 1) name[pos++] = entry->name[i];
    }
    if (entry->ext[0] != ' ') {
        if (pos < name_size - 1) name[pos++] = '.';
        for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            if (pos < name_size - 1) name[pos++] = entry->ext[i];
        }
    }
    name[pos] = '\0';
}

int fat_is_deleted_entry(const fat_dir_entry_t *entry) {
    return entry && entry->name[0] == FAT_DELETED_MARKER;
}
