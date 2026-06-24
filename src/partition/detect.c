#include "partition.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>

int pt_detect(device_t *dev, partition_table_t *table) {
    if (!dev || !table) return SALVAGE_ERR_INVALID;
    
    // Read MBR (LBA 0)
    uint8_t sector[512];
    int ret = device_read_sectors(dev, 0, 1, sector);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read sector 0");
        return ret;
    }
    
    // Check MBR signature
    uint16_t sig = read_le16(sector + 510);
    if (sig != 0x55AA) {
        LOG_WARN("No valid partition table found (missing 0x55AA signature)");
        return SALVAGE_ERR_NOT_FOUND;
    }
    
    // Check for GPT protective MBR
    // If partition 1 type is 0xEE, this is a GPT disk
    uint8_t part1_type = sector[450];  // Offset 0x1BE + 4 = type field
    if (part1_type == 0xEE) {
        LOG_INFO("Detected GPT protective MBR");
        ret = pt_parse_gpt(dev, table);
        if (ret == SALVAGE_OK) return ret;
        LOG_WARN("GPT parse failed, falling back to MBR");
    }
    
    // Try MBR
    ret = pt_parse_mbr(dev, table);
    if (ret == SALVAGE_OK && table->count > 0) {
        return SALVAGE_OK;
    }
    
    LOG_WARN("No partitions found");
    return SALVAGE_ERR_NOT_FOUND;
}

const char* fs_type_name(fs_type_t type) {
    switch (type) {
        case FS_TYPE_FAT12:      return "FAT12";
        case FS_TYPE_FAT16:      return "FAT16";
        case FS_TYPE_FAT32:      return "FAT32";
        case FS_TYPE_NTFS:       return "NTFS";
        case FS_TYPE_EXFAT:      return "exFAT";
        case FS_TYPE_LINUX:      return "Linux";
        case FS_TYPE_LINUX_SWAP: return "Linux Swap";
        case FS_TYPE_EXT2:       return "ext2";
        case FS_TYPE_EXT3:       return "ext3";
        case FS_TYPE_EXT4:       return "ext4";
        case FS_TYPE_HFS:        return "HFS+";
        case FS_TYPE_APFS:       return "APFS";
        default:                 return "Unknown";
    }
}

const char* mbr_type_name(uint8_t type_id) {
    switch (type_id) {
        case 0x00: return "Empty";
        case 0x01: return "FAT12";
        case 0x04: return "FAT16 <32M";
        case 0x05: return "Extended";
        case 0x06: return "FAT16";
        case 0x07: return "NTFS/exFAT";
        case 0x0B: return "FAT32 CHS";
        case 0x0C: return "FAT32 LBA";
        case 0x0E: return "FAT16 LBA";
        case 0x0F: return "Extended LBA";
        case 0x82: return "Linux Swap";
        case 0x83: return "Linux";
        case 0x85: return "Linux Extended";
        case 0xEE: return "GPT Protective";
        case 0xEF: return "EFI System";
        default:   return "Unknown";
    }
}
