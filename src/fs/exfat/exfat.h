#ifndef SALVAGE_EXFAT_H
#define SALVAGE_EXFAT_H

#include <salvage/salvage.h>
#include "device/device.h"
#include <stdint.h>

#define EXFAT_MAGIC             "EXFAT   "
#define EXFAT_DELETED_MARKER    0x05
#define EXFAT_ENTRY_FILE        0x85
#define EXFAT_ENTRY_STREAM      0xC0
#define EXFAT_ENTRY_NAME        0xC1
#define EXFAT_ATTR_READ_ONLY    0x01
#define EXFAT_ATTR_HIDDEN       0x02
#define EXFAT_ATTR_SYSTEM       0x04
#define EXFAT_ATTR_DIRECTORY    0x10
#define EXFAT_ATTR_ARCHIVE      0x20
#define EXFAT_CLUSTER_END       0xFFFFFFFF
#define EXFAT_CLUSTER_FREE      0x00000000

// exFAT Boot Sector - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];              // 0x00
    uint8_t  fs_name[8];           // 0x03: "EXFAT   "
    uint8_t  reserved1[53];        // 0x0B
    uint64_t partition_offset;     // 0x40
    uint64_t volume_length;        // 0x48
    uint32_t fat_offset;           // 0x50
    uint32_t fat_length;           // 0x58
    uint32_t cluster_heap_offset;  // 0x60
    uint32_t cluster_count;        // 0x68
    uint32_t first_cluster;        // 0x70: Root directory first cluster
    uint32_t volume_serial;        // 0x78
    uint16_t fs_revision;          // 0x80
    uint16_t volume_flags;         // 0x82
    uint8_t  bytes_per_sector_shift; // 0x84
    uint8_t  sectors_per_cluster_shift; // 0x85
    uint8_t  num_fats;             // 0x86
    uint8_t  drive_select;         // 0x87
    uint8_t  percent_in_use;       // 0x88
    uint8_t  reserved2[7];         // 0x89
    uint8_t  boot_code[390];       // 0x90
    uint16_t end_signature;        // 0x1FE: 0x55AA
} exfat_boot_t;
#pragma pack(pop)

// exFAT File Directory Entry (0x85) - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;           // 0x00: 0x85 (in-use) or 0x05 (deleted)
    uint8_t  secondary_count;      // 0x01
    uint16_t checksum;             // 0x02
    uint16_t attributes;           // 0x04
    uint16_t reserved1;            // 0x06
    uint32_t create_time;          // 0x08
    uint32_t modify_time;          // 0x0C
    uint32_t access_time;          // 0x10
    uint8_t  create_tenth;         // 0x14
    uint8_t  modify_tenth;         // 0x15
    uint8_t  create_tz;            // 0x16
    uint8_t  modify_tz;            // 0x17
    uint8_t  access_tz;            // 0x18
    uint8_t  reserved2[7];         // 0x19
} exfat_file_entry_t;
#pragma pack(pop)

// exFAT Stream Extension Entry (0xC0) - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;           // 0x00: 0xC0
    uint8_t  flags;                // 0x01
    uint8_t  reserved1;            // 0x02
    uint8_t  name_length;          // 0x03
    uint16_t name_hash;            // 0x04
    uint16_t reserved2;            // 0x06
    uint64_t valid_data_length;    // 0x08
    uint32_t reserved3;            // 0x10
    uint32_t first_cluster;        // 0x14
    uint64_t data_length;          // 0x18
} exfat_stream_entry_t;
#pragma pack(pop)

// exFAT File Name Entry (0xC1) - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;           // 0x00: 0xC1
    uint8_t  flags;                // 0x01
    uint16_t name[15];             // 0x02: UTF-16LE characters
} exfat_name_entry_t;
#pragma pack(pop)

// exFAT volume context
typedef struct {
    device_t *device;
    uint64_t partition_start;

    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t cluster_size;
    uint32_t fat_offset;           // FAT start (sectors from partition)
    uint32_t fat_length;           // FAT size (sectors)
    uint32_t cluster_heap_offset;  // Data area start (sectors from partition)
    uint32_t cluster_count;
    uint32_t root_dir_cluster;
    uint32_t num_fats;
    uint32_t serial_number;
} exfat_volume_t;

// Parsed file info
typedef struct {
    char name[256];
    uint32_t first_cluster;
    uint64_t data_length;
    uint64_t valid_data_length;
    uint64_t create_time;
    uint64_t modify_time;
    uint64_t access_time;
    uint16_t attributes;
    int      is_deleted;
} exfat_file_info_t;

// Initialize exFAT volume
int exfat_init(exfat_volume_t *vol, device_t *dev, uint64_t partition_start);

// Read a cluster
int exfat_read_cluster(exfat_volume_t *vol, uint32_t cluster, void *buf);

// Get next cluster from FAT
uint32_t exfat_next_cluster(exfat_volume_t *vol, uint32_t cluster);

// Read file data
int exfat_read_file_data(exfat_volume_t *vol, const exfat_file_info_t *info,
                         uint64_t offset, uint64_t size, void *buf);

// Scan for deleted files
int exfat_scan_deleted(exfat_volume_t *vol,
                       void (*callback)(exfat_file_info_t *info, void *ctx),
                       void *ctx);

#endif // SALVAGE_EXFAT_H
