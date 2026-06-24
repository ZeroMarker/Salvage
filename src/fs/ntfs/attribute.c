#include "ntfs.h"
#include "utils/endian.h"
#include "utils/str.h"
#include "utils/time.h"
#include "utils/log.h"
#include <string.h>

// Iterate through attributes in MFT record
// callback returns 0 to continue, non-zero to stop
static int iterate_attributes(const uint8_t *record, uint32_t record_size,
                              int (*callback)(const attr_header_t *attr, void *ctx),
                              void *ctx) {
    const mft_record_header_t *header = (const mft_record_header_t *)record;
    uint32_t offset = read_le16((const uint8_t *)&header->attrs_offset);
    
    while (offset + 4 <= record_size) {
        const attr_header_t *attr = (const attr_header_t *)(record + offset);
        
        // Check for end marker
        uint32_t type = read_le32((const uint8_t *)&attr->type);
        if (type == ATTR_TYPE_END || type == 0xFFFFFFFF) break;
        
        // Check for valid attribute length
        uint32_t length = read_le32((const uint8_t *)&attr->length);
        if (length < 24 || offset + length > record_size) break;
        
        int ret = callback(attr, ctx);
        if (ret != 0) return ret;
        
        offset += length;
    }
    
    return 0;
}

// Context for file info parsing
typedef struct {
    ntfs_file_info_t *info;
    int found_name;
    int found_data;
} parse_context_t;

static int parse_attribute(const attr_header_t *attr, void *ctx) {
    parse_context_t *pctx = (parse_context_t *)ctx;
    ntfs_file_info_t *info = pctx->info;
    
    uint32_t type = read_le32((const uint8_t *)&attr->type);
    uint8_t non_resident = attr->non_resident;
    
    switch (type) {
        case ATTR_TYPE_STANDARD_INFO: {
            if (non_resident) break;
            
            // Find attribute value
            uint16_t value_offset = read_le16((const uint8_t *)&((const attr_resident_t *)attr)->value_offset);
            const attr_standard_info_t *si = (const attr_standard_info_t *)((const uint8_t *)attr + value_offset);
            
            info->create_time = read_le64((const uint8_t *)&si->create_time);
            info->modify_time = read_le64((const uint8_t *)&si->modify_time);
            info->access_time = read_le64((const uint8_t *)&si->access_time);
            info->mft_time = read_le64((const uint8_t *)&si->mft_time);
            info->file_attributes = read_le32((const uint8_t *)&si->file_attributes);
            break;
        }
        
        case ATTR_TYPE_FILE_NAME: {
            if (non_resident) break;
            
            // Skip if we already found a Win32 name
            if (pctx->found_name) break;
            
            uint16_t value_offset = read_le16((const uint8_t *)&((const attr_resident_t *)attr)->value_offset);
            const attr_filename_t *fn = (const attr_filename_t *)((const uint8_t *)attr + value_offset);
            
            info->parent_mft = read_le64((const uint8_t *)&fn->parent_ref) & 0x0000FFFFFFFFFFFF;
            
            // Prefer Win32 name over DOS
            if (fn->name_type == FILE_NAME_DOS && pctx->found_name) break;
            
            // Convert name to UTF-8
            int name_len = fn->name_length;
            const uint16_t *name_ptr = (const uint16_t *)((const uint8_t *)fn + sizeof(attr_filename_t));
            
            utf16_to_utf8(name_ptr, name_len, info->name, sizeof(info->name));
            info->name_type = fn->name_type;
            
            if (fn->name_type == FILE_NAME_WIN32 || fn->name_type == FILE_NAME_POSIX) {
                pctx->found_name = 1;
            }
            
            // Update timestamps from filename attribute if standard info not found
            if (info->create_time == 0) {
                info->create_time = read_le64((const uint8_t *)&fn->create_time);
                info->modify_time = read_le64((const uint8_t *)&fn->modify_time);
                info->access_time = read_le64((const uint8_t *)&fn->access_time);
                info->mft_time = read_le64((const uint8_t *)&fn->mft_time);
            }
            
            if (info->allocated_size == 0) {
                info->allocated_size = read_le64((const uint8_t *)&fn->allocated_size);
                info->data_size = read_le64((const uint8_t *)&fn->data_size);
            }
            break;
        }
        
        case ATTR_TYPE_DATA: {
            // Skip named data streams (like $DATA:$I30)
            uint8_t name_length = attr->name_length;
            if (name_length > 0) break;
            
            if (non_resident) {
                const attr_non_resident_t *nr = (const attr_non_resident_t *)attr;
                info->allocated_size = read_le64((const uint8_t *)&nr->allocated_size);
                info->data_size = read_le64((const uint8_t *)&nr->data_size);
                info->is_resident = 0;
                
                // Store data run offset for later use
                uint16_t data_runs_offset = read_le16((const uint8_t *)&nr->data_runs_offset);
                info->data_lcn = (uint64_t)((const uint8_t *)attr - pctx->info->mft_number * 1024) + data_runs_offset;
            } else {
                // Resident data - store offset
                uint16_t value_offset = read_le16((const uint8_t *)&((const attr_resident_t *)attr)->value_offset);
                uint32_t value_length = read_le32((const uint8_t *)&((const attr_resident_t *)attr)->value_length);
                
                info->data_size = value_length;
                info->allocated_size = value_length;
                info->is_resident = 1;
                info->data_lcn = (uint64_t)value_offset;  // Offset within record
            }
            pctx->found_data = 1;
            break;
        }
    }
    
    return 0;  // Continue iteration
}

int ntfs_parse_file_info(const uint8_t *record, ntfs_file_info_t *info) {
    if (!record || !info) return SALVAGE_ERR_INVALID;
    
    const mft_record_header_t *header = (const mft_record_header_t *)record;
    uint32_t record_size = read_le32((const uint8_t *)&header->bytes_allocated);
    
    memset(info, 0, sizeof(ntfs_file_info_t));
    
    // Parse header
    info->mft_number = read_le32((const uint8_t *)&header->mft_number);
    info->sequence = read_le16((const uint8_t *)&header->sequence_number);
    info->flags = read_le16((const uint8_t *)&header->flags);
    
    // Parse attributes
    parse_context_t ctx = { .info = info, .found_name = 0, .found_data = 0 };
    iterate_attributes(record, record_size, parse_attribute, &ctx);
    
    // Mark as deleted if not in use
    if (!(info->flags & MFT_FLAG_IN_USE)) {
        info->file_attributes |= 0x80000000;  // Custom flag for deleted
    }
    
    return SALVAGE_OK;
}
