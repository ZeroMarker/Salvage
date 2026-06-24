#include "partition.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdio.h>

// MBR partition entry structure
#pragma pack(push, 1)
typedef struct {
    uint8_t  boot_flag;       // 0x80 = bootable, 0x00 = non-bootable
    uint8_t  chs_start[3];    // CHS of first sector
    uint8_t  type;            // Partition type
    uint8_t  chs_end[3];      // CHS of last sector
    uint32_t lba_start;       // LBA of first sector (little-endian)
    uint32_t sectors;         // Number of sectors (little-endian)
} mbr_entry_t;

typedef struct {
    uint8_t       boot_code[446];
    mbr_entry_t   entries[4];
    uint16_t      signature;  // 0x55AA
} mbr_t;
#pragma pack(pop)

static fs_type_t detect_fs_from_mbr_type(uint8_t type) {
    switch (type) {
        case 0x01: return FS_TYPE_FAT12;
        case 0x04:
        case 0x06:
        case 0x0E: return FS_TYPE_FAT16;
        case 0x0B:
        case 0x0C: return FS_TYPE_FAT32;
        case 0x07: return FS_TYPE_NTFS;  // Could also be exFAT
        case 0x82: return FS_TYPE_LINUX_SWAP;
        case 0x83: return FS_TYPE_LINUX;
        default:   return FS_TYPE_UNKNOWN;
    }
}

int pt_parse_mbr(device_t *dev, partition_table_t *table) {
    if (!dev || !table) return SALVAGE_ERR_INVALID;
    
    memset(table, 0, sizeof(partition_table_t));
    table->device = dev;
    table->type = PT_TYPE_MBR;
    
    // Read MBR sector
    uint8_t sector[512];
    int ret = device_read_sectors(dev, 0, 1, sector);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read MBR");
        return ret;
    }
    
    // Check signature
    uint16_t sig = read_le16(sector + 510);
    if (sig != 0x55AA) {
        LOG_DEBUG("No MBR signature found (0x%04X)", sig);
        return SALVAGE_ERR_NOT_FOUND;
    }
    
    // Parse entries
    const mbr_t *mbr = (const mbr_t *)sector;
    
    for (int i = 0; i < 4; i++) {
        const mbr_entry_t *entry = &mbr->entries[i];
        
        if (entry->type == 0x00) continue;  // Empty slot
        
        partition_t *part = &table->entries[table->count];
        part->index = table->count;
        part->type_id = entry->type;
        part->start_lba = read_le32((const uint8_t *)&entry->lba_start);
        part->size_sectors = read_le32((const uint8_t *)&entry->sectors);
        part->size_bytes = part->size_sectors * dev->sector_size;
        part->is_bootable = (entry->boot_flag == 0x80);
        part->fs_type = detect_fs_from_mbr_type(entry->type);
        
        snprintf(part->name, sizeof(part->name), "Partition %d", i + 1);
        
        LOG_DEBUG("MBR partition %d: type=0x%02X, start=%llu, size=%llu sectors",
                  i, entry->type, part->start_lba, part->size_sectors);
        
        table->count++;
    }
    
    // Check for extended partitions (EBR chain)
    for (int i = 0; i < 4; i++) {
        uint8_t type = mbr->entries[i].type;
        if (type == 0x05 || type == 0x0F) {
            // Extended partition - parse EBR chain
            uint64_t ebr_lba = read_le32((const uint8_t *)&mbr->entries[i].lba_start);
            uint64_t ext_base = ebr_lba;
            int logical_index = 0;
            
            while (ebr_lba != 0) {
                uint8_t ebr_sector[512];
                ret = device_read_sectors(dev, ebr_lba, 1, ebr_sector);
                if (ret != SALVAGE_OK) break;
                
                uint16_t ebr_sig = read_le16(ebr_sector + 510);
                if (ebr_sig != 0x55AA) break;
                
                const mbr_entry_t *log_entry = (const mbr_entry_t *)(ebr_sector + 446);
                const mbr_entry_t *next_entry = (const mbr_entry_t *)(ebr_sector + 462);
                
                if (log_entry->type != 0x00) {
                    partition_t *part = &table->entries[table->count];
                    part->index = table->count;
                    part->type_id = log_entry->type;
                    part->start_lba = ebr_lba + read_le32((const uint8_t *)&log_entry->lba_start);
                    part->size_sectors = read_le32((const uint8_t *)&log_entry->sectors);
                    part->size_bytes = part->size_sectors * dev->sector_size;
                    part->fs_type = detect_fs_from_mbr_type(log_entry->type);
                    
                    snprintf(part->name, sizeof(part->name), "Logical %d", logical_index + 1);
                    
                    LOG_DEBUG("EBR logical %d: type=0x%02X, start=%llu",
                              logical_index, log_entry->type, part->start_lba);
                    
                    table->count++;
                    logical_index++;
                }
                
                // Follow chain
                uint32_t next_lba = read_le32((const uint8_t *)&next_entry->lba_start);
                if (next_lba == 0 || next_entry->type == 0x00) break;
                ebr_lba = ext_base + next_lba;
            }
        }
    }
    
    LOG_INFO("MBR: Found %d partitions", table->count);
    return SALVAGE_OK;
}
