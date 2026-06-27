#ifndef SALVAGE_FAT_H
#define SALVAGE_FAT_H

#include <salvage/salvage.h>
#include "device/device.h"
#include <stdint.h>

#define FAT_DIR_ENTRY_SIZE      32
#define FAT_LFN_ENTRY_SIZE      32
#define FAT_MAX_LFN_ENTRIES     20
#define FAT_DELETED_MARKER      0xE5
#define FAT_ATTR_READ_ONLY      0x01
#define FAT_ATTR_HIDDEN         0x02
#define FAT_ATTR_SYSTEM         0x04
#define FAT_ATTR_VOLUME_ID      0x08
#define FAT_ATTR_DIRECTORY      0x10
#define FAT_ATTR_ARCHIVE        0x20
#define FAT_ATTR_LFN            0x0F
#define FAT_CLUSTER_END         0x0FFFFFF8
#define FAT_CLUSTER_BAD         0x0FFFFFF7
#define FAT_CLUSTER_FREE        0x00000000

// FAT32 Boot Sector (BPB) - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];              // 0x00: Jump instruction
    uint8_t  oem[8];               // 0x03: OEM name
    uint16_t bytes_per_sector;     // 0x0B: Bytes per sector (512)
    uint8_t  sectors_per_cluster;  // 0x0D: Sectors per cluster
    uint16_t reserved_sectors;     // 0x0E: Reserved sectors
    uint8_t  num_fats;             // 0x10: Number of FATs
    uint16_t root_entry_count;     // 0x11: Root entry count (0 for FAT32)
    uint16_t total_sectors_16;     // 0x13: Total sectors (0 for FAT32)
    uint8_t  media_type;           // 0x15: Media type
    uint16_t fat_size_16;          // 0x16: FAT size (0 for FAT32)
    uint16_t sectors_per_track;    // 0x18: Sectors per track
    uint16_t num_heads;            // 0x1A: Number of heads
    uint32_t hidden_sectors;       // 0x1C: Hidden sectors
    uint32_t total_sectors_32;     // 0x20: Total sectors
    uint32_t fat_size_32;          // 0x24: FAT size (sectors)
    uint16_t ext_flags;            // 0x28: Extended flags
    uint16_t fs_version;           // 0x2A: FS version
    uint32_t root_cluster;         // 0x2C: Root directory cluster
    uint16_t fs_info_sector;       // 0x30: FS info sector
    uint16_t backup_boot_sector;   // 0x32: Backup boot sector
    uint8_t  reserved[12];         // 0x34: Reserved
    uint8_t  drive_number;         // 0x40: Drive number
    uint8_t  reserved1;            // 0x41: Reserved
    uint8_t  boot_signature;       // 0x42: Boot signature (0x29)
    uint32_t volume_serial;        // 0x43: Volume serial number
    char     volume_label[11];     // 0x47: Volume label
    char     fs_type[8];           // 0x52: "FAT32   "
    uint8_t  boot_code[420];       // 0x5A: Boot code
    uint16_t end_signature;        // 0x1FE: 0x55AA
} fat_boot_t;
#pragma pack(pop)

// FAT directory entry (short name) - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  name[8];              // 0x00: Short name
    uint8_t  ext[3];               // 0x08: Short extension
    uint8_t  attributes;           // 0x0B: Attributes
    uint8_t  reserved;             // 0x0C: Reserved (NT case)
    uint8_t  create_time_tenth;    // 0x0D: Creation time (tenths)
    uint16_t create_time;          // 0x0E: Creation time
    uint16_t create_date;          // 0x10: Creation date
    uint16_t access_date;          // 0x12: Last access date
    uint16_t first_cluster_hi;     // 0x14: First cluster (high word)
    uint16_t modify_time;          // 0x16: Last modification time
    uint16_t modify_date;          // 0x18: Last modification date
    uint16_t first_cluster_lo;     // 0x1A: First cluster (low word)
    uint32_t file_size;            // 0x1C: File size in bytes
} fat_dir_entry_t;
#pragma pack(pop)

// FAT LFN entry - packed
#pragma pack(push, 1)
typedef struct {
    uint8_t  order;                // 0x00: Order (0x40 = last)
    uint16_t name1[5];             // 0x01: Characters 1-5 (UTF-16LE)
    uint8_t  attributes;           // 0x0B: 0x0F
    uint8_t  type;                 // 0x0C: 0x00
    uint8_t  checksum;             // 0x0D: Checksum
    uint16_t name2[6];             // 0x0E: Characters 6-11
    uint16_t zero;                 // 0x1A: 0x0000
    uint16_t name3[2];             // 0x1C: Characters 12-13
} fat_lfn_entry_t;
#pragma pack(pop)

// FAT volume context
typedef struct {
    device_t *device;
    uint64_t partition_start;

    uint32_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t cluster_size;
    uint32_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size_sectors;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t total_clusters;
    uint32_t serial_number;
} fat_volume_t;

// Parsed file info
typedef struct {
    char name[256];
    char short_name[13];
    uint32_t first_cluster;
    uint64_t file_size;
    uint32_t create_date;
    uint32_t create_time;
    uint32_t modify_date;
    uint32_t modify_time;
    uint32_t access_date;
    uint8_t  attributes;
    int      is_deleted;
    uint64_t entry_offset;         // Disk offset of directory entry
} fat_file_info_t;

// Initialize FAT32 volume
int fat_init(fat_volume_t *vol, device_t *dev, uint64_t partition_start);

// Read a cluster
int fat_read_cluster(fat_volume_t *vol, uint32_t cluster, void *buf);

// Get next cluster in chain
uint32_t fat_next_cluster(fat_volume_t *vol, uint32_t cluster);

// Read data from file (following FAT chain)
int fat_read_file_data(fat_volume_t *vol, const fat_file_info_t *info,
                       uint64_t offset, uint64_t size, void *buf);

// Parse short name from directory entry
void fat_parse_short_name(const fat_dir_entry_t *entry, char *name, int name_size);

// Check if directory entry is deleted
int fat_is_deleted_entry(const fat_dir_entry_t *entry);

// Scan directory tree for deleted files, call callback for each
int fat_scan_deleted(fat_volume_t *vol,
                     void (*callback)(fat_file_info_t *info, void *ctx),
                     void *ctx);

#endif // SALVAGE_FAT_H
