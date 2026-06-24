#ifndef SALVAGE_NTFS_H
#define SALVAGE_NTFS_H

#include <salvage/salvage.h>
#include "device/device.h"
#include <stdint.h>

// NTFS constants
#define NTFS_MAGIC             "NTFS    "
#define NTFS_SECTOR_SIZE       512
#define NTFS_MFT_RECORD_SIZE   1024
#define NTFS_MFT_MAGIC         "FILE"
#define NTFS_INDEX_MAGIC       "INDX"

// MFT Record flags
#define MFT_FLAG_IN_USE        0x01
#define MFT_FLAG_DIRECTORY     0x02

// Attribute types
#define ATTR_TYPE_STANDARD_INFO    0x10
#define ATTR_TYPE_ATTRIBUTE_LIST   0x20
#define ATTR_TYPE_FILE_NAME        0x30
#define ATTR_TYPE_OBJECT_ID        0x40
#define ATTR_TYPE_SECURITY_DESC    0x50
#define ATTR_TYPE_VOLUME_NAME      0x60
#define ATTR_TYPE_VOLUME_INFO      0x70
#define ATTR_TYPE_DATA             0x80
#define ATTR_TYPE_INDEX_ROOT       0x90
#define ATTR_TYPE_INDEX_ALLOC      0xA0
#define ATTR_TYPE_BITMAP           0xB0
#define ATTR_TYPE_REPARSE_POINT   0xC0
#define ATTR_TYPE_EA_INFO          0xD0
#define ATTR_TYPE_EA               0xE0
#define ATTR_TYPE_END              0xFFFFFFFF

// File name types
#define FILE_NAME_POSIX    0x00
#define FILE_NAME_WIN32    0x01
#define FILE_NAME_DOS      0x02

// Special MFT entries
#define MFT_ENTRY_MFT       0
#define MFT_ENTRY_MFTMIRR   1
#define MFT_ENTRY_LOGFILE   2
#define MFT_ENTRY_VOLUME    3
#define MFT_ENTRY_ATTRDEF   4
#define MFT_ENTRY_ROOT      5
#define MFT_ENTRY_BITMAP    6
#define MFT_ENTRY_BOOT      7
#define MFT_ENTRY_BADCLUS   8
#define MFT_ENTRY_SECURE    9
#define MFT_ENTRY_UPCASE   10
#define MFT_ENTRY_EXTEND   11
#define MFT_ENTRY_FIRST_USER 16

// NTFS Boot Sector
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];             // 0x00: Jump instruction
    uint8_t  oem[8];              // 0x03: OEM name "NTFS    "
    uint16_t bytes_per_sector;    // 0x0B: Bytes per sector
    uint8_t  sectors_per_cluster; // 0x0D: Sectors per cluster
    uint16_t reserved_sectors;    // 0x0E: Reserved sectors
    uint8_t  fats_count;          // 0x10: Number of FATs (0)
    uint16_t root_entries;        // 0x11: Root entries (0)
    uint16_t total_sectors_16;    // 0x13: Total sectors (0)
    uint8_t  media_type;          // 0x15: Media type
    uint16_t sectors_per_fat;    // 0x16: Sectors per FAT (0)
    uint16_t sectors_per_track;  // 0x18: Sectors per track
    uint16_t heads_count;        // 0x1A: Number of heads
    uint32_t hidden_sectors;     // 0x1C: Hidden sectors
    uint32_t total_sectors_32;   // 0x20: Total sectors (0)
    uint8_t  drive_number;       // 0x24: Drive number
    uint8_t  reserved1;          // 0x25: Reserved
    uint8_t  boot_signature;     // 0x26: Extended boot signature (0x29)
    uint8_t  reserved2[3];       // 0x27: Reserved
    uint64_t total_sectors;      // 0x28: Total sectors
    uint64_t mft_lba;            // 0x30: MFT LBA
    uint64_t mft_mirror_lba;     // 0x38: MFT mirror LBA
    int8_t   clusters_per_mft;   // 0x40: Clusters per MFT record
    uint8_t  reserved3[3];       // 0x41: Reserved
    int8_t   clusters_per_index; // 0x44: Clusters per index buffer
    uint8_t  reserved4[3];       // 0x45: Reserved
    uint64_t serial_number;      // 0x48: Volume serial number
    uint32_t checksum;           // 0x50: Checksum
    uint8_t  boot_code[426];     // 0x54: Boot code
    uint16_t end_signature;      // 0x1FE: 0x55AA
} ntfs_boot_t;
#pragma pack(pop)

// MFT Record Header
#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[4];       // "FILE"
    uint16_t update_offset;      // Offset to update sequence
    uint16_t update_size;        // Size in words of update sequence
    uint64_t logfile_seq;        // $LogFile sequence number
    uint16_t sequence_number;    // Sequence number
    uint16_t hard_link_count;    // Hard link count
    uint16_t attrs_offset;       // Offset to first attribute
    uint16_t flags;              // Flags (0x01=in use, 0x02=directory)
    uint32_t bytes_used;         // Bytes used in record
    uint32_t bytes_allocated;    // Bytes allocated for record
    uint64_t base_mft;           // Base MFT record reference
    uint16_t next_attr_id;       // Next attribute ID
    uint16_t reserved;           // Reserved (XP+)
    uint32_t mft_number;         // MFT record number (XP+)
} mft_record_header_t;
#pragma pack(pop)

// Attribute Header
#pragma pack(push, 1)
typedef struct {
    uint32_t type;               // Attribute type
    uint32_t length;             // Total length
    uint8_t  non_resident;       // 0=resident, 1=non-resident
    uint8_t  name_length;        // Name length in words
    uint16_t name_offset;        // Offset to name
    uint16_t flags;              // Attribute flags
    uint16_t attr_id;            // Attribute ID
} attr_header_t;
#pragma pack(pop)

// Resident Attribute Header (extends attr_header_t)
#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
    uint32_t value_length;       // Length of attribute value
    uint16_t value_offset;       // Offset to attribute value
    uint8_t  indexed;            // Indexed flag
    uint8_t  reserved;
} attr_resident_t;
#pragma pack(pop)

// Non-Resident Attribute Header
#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
    uint64_t lowest_vcn;         // Lowest VCN
    uint64_t highest_vcn;        // Highest VCN
    uint16_t data_runs_offset;   // Offset to data runs
    uint16_t compression_unit;   // Compression unit size
    uint8_t  reserved[4];        // Reserved
    uint64_t allocated_size;     // Allocated size
    uint64_t data_size;          // Data size
    uint64_t initialized_size;   // Initialized size
} attr_non_resident_t;
#pragma pack(pop)

// Standard Information Attribute ($10)
#pragma pack(push, 1)
typedef struct {
    uint64_t create_time;        // File creation time
    uint64_t modify_time;        // File modification time
    uint64_t mft_time;           // MFT modification time
    uint64_t access_time;        // File access time
    uint32_t file_attributes;    // File attributes
    uint32_t max_versions;       // Maximum versions
    uint32_t version;            // Version number
    uint32_t class_id;           // Class ID
    uint32_t owner_id;           // Owner ID (XP+)
    uint32_t security_id;        // Security ID (XP+)
    uint64_t quota_charge;       // Quota charge (XP+)
    uint64_t usn;                // Update sequence number (XP+)
} attr_standard_info_t;
#pragma pack(pop)

// File Name Attribute ($30)
#pragma pack(push, 1)
typedef struct {
    uint64_t parent_ref;         // Parent directory MFT reference
    uint64_t create_time;        // File creation time
    uint64_t modify_time;        // File modification time
    uint64_t mft_time;           // MFT modification time
    uint64_t access_time;        // File access time
    uint64_t allocated_size;     // Allocated size
    uint64_t data_size;          // Data size
    uint32_t file_attributes;    // File attributes
    uint32_t ea_flags;           // EA and reparse flags
    uint8_t  name_length;        // Name length in characters
    uint8_t  name_type;          // Name type (POSIX/Win32/DOS)
    // uint16_t name[];          // File name (UTF-16LE)
} attr_filename_t;
#pragma pack(pop)

// Parsed file information
typedef struct {
    uint64_t mft_number;         // MFT record number
    uint16_t sequence;           // Sequence number
    uint32_t flags;              // MFT flags
    uint32_t file_attributes;   // File attributes
    
    // File name
    char name[256];              // UTF-8 file name
    uint8_t name_type;           // Name type
    
    // Timestamps (NTFS format)
    uint64_t create_time;
    uint64_t modify_time;
    uint64_t access_time;
    uint64_t mft_time;
    
    // Sizes
    uint64_t allocated_size;
    uint64_t data_size;
    
    // Data location
    uint64_t data_lcn;           // Logical cluster number of data
    uint64_t data_size_clusters; // Data size in clusters
    int      is_resident;        // Data is resident in MFT
    
    // Parent directory
    uint64_t parent_mft;         // Parent directory MFT number
} ntfs_file_info_t;

// NTFS volume context
typedef struct {
    device_t *device;
    uint64_t partition_start;    // Partition start LBA
    
    // Boot sector info
    uint32_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint32_t cluster_size;       // Bytes per cluster
    uint64_t total_sectors;
    uint64_t mft_lba;
    uint64_t mft_mirror_lba;
    uint32_t mft_record_size;    // Bytes per MFT record
    uint32_t index_block_size;   // Bytes per index block
    uint64_t serial_number;
} ntfs_volume_t;

// Initialize NTFS volume from partition
int ntfs_init(ntfs_volume_t *vol, device_t *dev, uint64_t partition_start);

// Read MFT record
int ntfs_read_mft_record(ntfs_volume_t *vol, uint64_t mft_number, uint8_t *record);

// Parse file info from MFT record
int ntfs_parse_file_info(const uint8_t *record, ntfs_file_info_t *info);

// Read data from file (non-resident)
int ntfs_read_file_data(ntfs_volume_t *vol, const ntfs_file_info_t *info,
                        uint64_t offset, uint64_t size, void *buf);

// Check if MFT record is valid
int ntfs_is_valid_mft_record(const uint8_t *record);

// Check if MFT record is deleted (not in use)
int ntfs_is_deleted_record(const uint8_t *record);

#endif // SALVAGE_NTFS_H
