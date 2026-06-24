#ifndef SALVAGE_PARTITION_H
#define SALVAGE_PARTITION_H

#include <salvage/salvage.h>
#include "device/device.h"
#include <stdint.h>

// Partition type identifiers
typedef enum {
    FS_TYPE_UNKNOWN = 0,
    FS_TYPE_FAT12,
    FS_TYPE_FAT16,
    FS_TYPE_FAT32,
    FS_TYPE_NTFS,
    FS_TYPE_EXFAT,
    FS_TYPE_LINUX,
    FS_TYPE_LINUX_SWAP,
    FS_TYPE_EXT2,
    FS_TYPE_EXT3,
    FS_TYPE_EXT4,
    FS_TYPE_HFS,
    FS_TYPE_APFS,
} fs_type_t;

// Partition table type
typedef enum {
    PT_TYPE_NONE = 0,
    PT_TYPE_MBR,
    PT_TYPE_GPT,
} pt_type_t;

// Partition entry
typedef struct partition {
    uint64_t start_lba;       // Start LBA
    uint64_t size_sectors;    // Size in sectors
    uint64_t size_bytes;      // Size in bytes
    uint8_t  type_id;         // Raw type ID
    fs_type_t fs_type;        // Detected filesystem type
    uint8_t  is_bootable;     // Bootable flag
    int      index;           // Partition index (0-based)
    char     name[64];        // Partition name (GPT)
    char     label[64];       // Volume label
} partition_t;

// Partition table
typedef struct partition_table {
    pt_type_t    type;         // MBR or GPT
    int          count;        // Number of partitions
    partition_t  entries[128]; // Partition entries
    device_t    *device;       // Associated device
} partition_table_t;

// Parse MBR partition table
int pt_parse_mbr(device_t *dev, partition_table_t *table);

// Parse GPT partition table
int pt_parse_gpt(device_t *dev, partition_table_t *table);

// Auto-detect and parse partition table
int pt_detect(device_t *dev, partition_table_t *table);

// Get filesystem type name
const char* fs_type_name(fs_type_t type);

// Get partition type name from MBR type ID
const char* mbr_type_name(uint8_t type_id);

#endif // SALVAGE_PARTITION_H
