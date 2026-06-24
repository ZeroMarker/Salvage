#include "ntfs.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>

int ntfs_init(ntfs_volume_t *vol, device_t *dev, uint64_t partition_start) {
    if (!vol || !dev) return SALVAGE_ERR_INVALID;
    
    memset(vol, 0, sizeof(ntfs_volume_t));
    vol->device = dev;
    vol->partition_start = partition_start;
    
    // Read boot sector
    uint8_t boot_buf[512];
    int ret = device_read_sectors(dev, partition_start, 1, boot_buf);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read NTFS boot sector");
        return ret;
    }
    
    const ntfs_boot_t *boot = (const ntfs_boot_t *)boot_buf;
    
    // Verify OEM signature
    if (memcmp(boot->oem, NTFS_MAGIC, 8) != 0) {
        LOG_ERROR("Not an NTFS volume (OEM: %.8s)", boot->oem);
        return SALVAGE_ERR_INVALID;
    }
    
    // Verify end signature
    if (read_le16(boot_buf + 510) != 0x55AA) {
        LOG_WARN("NTFS boot sector missing end signature");
    }
    
    // Parse boot sector
    vol->bytes_per_sector = read_le16(boot_buf + 0x0B);
    vol->sectors_per_cluster = boot_buf[0x0D];
    vol->total_sectors = read_le64(boot_buf + 0x28);
    vol->mft_lba = read_le64(boot_buf + 0x30);
    vol->mft_mirror_lba = read_le64(boot_buf + 0x38);
    vol->serial_number = read_le64(boot_buf + 0x48);
    
    // Calculate cluster size
    vol->cluster_size = vol->bytes_per_sector * vol->sectors_per_cluster;
    
    // Calculate MFT record size
    int8_t clusters_per_mft = (int8_t)boot_buf[0x40];
    if (clusters_per_mft > 0) {
        vol->mft_record_size = clusters_per_mft * vol->cluster_size;
    } else {
        // Negative value means 2^(-clusters_per_mft) bytes
        vol->mft_record_size = 1 << (-clusters_per_mft);
    }
    
    // Calculate index block size
    int8_t clusters_per_index = (int8_t)boot_buf[0x44];
    if (clusters_per_index > 0) {
        vol->index_block_size = clusters_per_index * vol->cluster_size;
    } else {
        vol->index_block_size = 1 << (-clusters_per_index);
    }
    
    // Adjust MFT LBA relative to partition start
    vol->mft_lba += partition_start;
    vol->mft_mirror_lba += partition_start;
    
    LOG_INFO("NTFS volume initialized:");
    LOG_INFO("  Bytes/sector: %u", vol->bytes_per_sector);
    LOG_INFO("  Sectors/cluster: %u", vol->sectors_per_cluster);
    LOG_INFO("  Cluster size: %u bytes", vol->cluster_size);
    LOG_INFO("  Total sectors: %llu", vol->total_sectors);
    LOG_INFO("  MFT LBA: %llu", vol->mft_lba);
    LOG_INFO("  MFT record size: %u bytes", vol->mft_record_size);
    LOG_INFO("  Serial: %016llX", vol->serial_number);
    
    return SALVAGE_OK;
}

int ntfs_is_valid_mft_record(const uint8_t *record) {
    if (!record) return 0;
    return memcmp(record, NTFS_MFT_MAGIC, 4) == 0;
}

int ntfs_is_deleted_record(const uint8_t *record) {
    if (!ntfs_is_valid_mft_record(record)) return 0;
    
    const mft_record_header_t *header = (const mft_record_header_t *)record;
    uint16_t flags = read_le16((const uint8_t *)&header->flags);
    
    // Deleted = signature is FILE but IN_USE flag is not set
    return (flags & MFT_FLAG_IN_USE) == 0;
}
