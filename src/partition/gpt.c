#include "partition.h"
#include "utils/endian.h"
#include "utils/log.h"
#include "utils/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_WINDOWS
#define strcasecmp _stricmp
#endif

// GPT constants
#define GPT_SIGNATURE "EFI PART"
#define GPT_HEADER_LBA 1
#define GPT_ENTRY_SIZE 128
#define GPT_MAX_ENTRIES 128

#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[8];       // "EFI PART"
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entries_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
} gpt_header_t;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];           // UTF-16LE
} gpt_entry_t;
#pragma pack(pop)

// Well-known partition type GUIDs
typedef struct {
    const char *guid;
    const char *name;
    fs_type_t   fs_type;
} guid_info_t;

static const guid_info_t known_guids[] = {
    // EFI System Partition
    {"C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "EFI System", FS_TYPE_UNKNOWN},
    // Microsoft Basic Data
    {"EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Microsoft Basic Data", FS_TYPE_NTFS},
    // Microsoft Reserved
    {"E3C9E316-0B5C-4DB8-817D-F92DF00215AE", "Microsoft Reserved", FS_TYPE_UNKNOWN},
    // Windows Recovery
    {"DE94BBA4-06D1-4D40-A16A-BFD50179D6AC", "Windows Recovery", FS_TYPE_NTFS},
    // Linux Filesystem
    {"0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Linux Filesystem", FS_TYPE_LINUX},
    // Linux Swap
    {"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Linux Swap", FS_TYPE_LINUX_SWAP},
    // Linux LVM
    {"E6D6D379-F507-44C2-A23C-238F2A3DF928", "Linux LVM", FS_TYPE_UNKNOWN},
    // HFS+
    {"48465300-0000-11AA-AA11-00306543ECAC", "HFS+", FS_TYPE_HFS},
    // APFS
    {"7C3457EF-0000-11AA-AA11-00306543ECAC", "APFS", FS_TYPE_APFS},
    {NULL, NULL, FS_TYPE_UNKNOWN}
};

static void guid_to_string(const uint8_t *guid, char *buf, size_t buf_size) {
    uint32_t d1 = read_le32(guid);
    uint16_t d2 = read_le16(guid + 4);
    uint16_t d3 = read_le16(guid + 6);
    
    snprintf(buf, buf_size, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             d1, d2, d3,
             guid[8], guid[9], guid[10], guid[11],
             guid[12], guid[13], guid[14], guid[15]);
}

static const guid_info_t* find_guid_info(const char *guid_str) {
    for (int i = 0; known_guids[i].guid != NULL; i++) {
        if (strcasecmp(guid_str, known_guids[i].guid) == 0) {
            return &known_guids[i];
        }
    }
    return NULL;
}

int pt_parse_gpt(device_t *dev, partition_table_t *table) {
    if (!dev || !table) return SALVAGE_ERR_INVALID;
    
    memset(table, 0, sizeof(partition_table_t));
    table->device = dev;
    table->type = PT_TYPE_GPT;
    
    // Read GPT header (LBA 1)
    uint8_t header_sector[512];
    int ret = device_read_sectors(dev, GPT_HEADER_LBA, 1, header_sector);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read GPT header");
        return ret;
    }
    
    // Verify signature
    if (memcmp(header_sector, GPT_SIGNATURE, 8) != 0) {
        LOG_DEBUG("No GPT signature found");
        return SALVAGE_ERR_NOT_FOUND;
    }
    
    const gpt_header_t *header = (const gpt_header_t *)header_sector;
    
    LOG_DEBUG("GPT: revision=%u, entries=%u, entry_size=%u",
              header->revision, header->num_partition_entries, header->partition_entry_size);
    
    // Read partition entries
    uint64_t entries_lba = header->partition_entries_lba;
    uint32_t num_entries = header->num_partition_entries;
    uint32_t entry_size = header->partition_entry_size;
    
    if (entry_size == 0) entry_size = GPT_ENTRY_SIZE;
    if (num_entries > GPT_MAX_ENTRIES) num_entries = GPT_MAX_ENTRIES;
    
    // Calculate sectors needed for entries
    uint32_t entries_bytes = num_entries * entry_size;
    uint32_t entries_sectors = (entries_bytes + dev->sector_size - 1) / dev->sector_size;
    
    uint8_t *entries_buf = malloc(entries_sectors * dev->sector_size);
    if (!entries_buf) return SALVAGE_ERR_NO_MEMORY;
    
    ret = device_read_sectors(dev, entries_lba, entries_sectors, entries_buf);
    if (ret != SALVAGE_OK) {
        free(entries_buf);
        LOG_ERROR("Failed to read GPT partition entries");
        return ret;
    }
    
    // Parse entries
    for (uint32_t i = 0; i < num_entries && table->count < 128; i++) {
        const gpt_entry_t *entry = (const gpt_entry_t *)(entries_buf + i * entry_size);
        
        // Check if entry is used (type GUID not all zeros)
        uint8_t zero_guid[16] = {0};
        if (memcmp(entry->type_guid, zero_guid, 16) == 0) continue;
        
        partition_t *part = &table->entries[table->count];
        part->index = table->count;
        part->start_lba = entry->first_lba;
        part->size_sectors = entry->last_lba - entry->first_lba + 1;
        part->size_bytes = part->size_sectors * dev->sector_size;
        
        // Convert type GUID to string
        char guid_str[40];
        guid_to_string(entry->type_guid, guid_str, sizeof(guid_str));
        
        // Look up GUID info
        const guid_info_t *info = find_guid_info(guid_str);
        if (info) {
            strncpy(part->name, info->name, sizeof(part->name) - 1);
            part->fs_type = info->fs_type;
        } else {
            snprintf(part->name, sizeof(part->name), "Unknown (%s)", guid_str);
            part->fs_type = FS_TYPE_UNKNOWN;
        }
        
        // Convert partition name from UTF-16LE
        utf16_to_utf8(entry->name, 36, part->label, sizeof(part->label));
        
        // Store raw type ID (we'll use index into GUID)
        part->type_id = 0x07;  // GPT always uses 0x07 style
        
        LOG_DEBUG("GPT entry %u: type=%s, start=%llu, end=%llu",
                  i, guid_str, entry->first_lba, entry->last_lba);
        
        table->count++;
    }
    
    free(entries_buf);
    
    LOG_INFO("GPT: Found %d partitions", table->count);
    return SALVAGE_OK;
}
