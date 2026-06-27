#include "ntfs.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>

// Parse data runs and read file data
int ntfs_read_file_data(ntfs_volume_t *vol, const ntfs_file_info_t *info,
                        uint64_t offset, uint64_t size, void *buf) {
    if (!vol || !info || !buf) return SALVAGE_ERR_INVALID;
    if (size == 0) return SALVAGE_OK;
    
    // For resident data, read directly from MFT record
    if (info->is_resident) {
        if (offset + size > info->data_size) {
            LOG_ERROR("Read beyond resident data size");
            return SALVAGE_ERR_IO;
        }
        
        // Re-read MFT record to get resident data
        uint8_t *record = malloc(vol->mft_record_size);
        if (!record) return SALVAGE_ERR_NO_MEMORY;
        
        int ret = ntfs_read_mft_record(vol, info->mft_number, record);
        if (ret != SALVAGE_OK) {
            free(record);
            return ret;
        }
        
        // Find $DATA attribute and copy value
        const mft_record_header_t *header = (const mft_record_header_t *)record;
        uint32_t attr_offset = read_le16((const uint8_t *)&header->attrs_offset);
        uint32_t record_size = read_le32((const uint8_t *)&header->bytes_allocated);
        
        while (attr_offset + 4 <= record_size) {
            const attr_header_t *attr = (const attr_header_t *)(record + attr_offset);
            uint32_t type = read_le32((const uint8_t *)&attr->type);
            
            if (type == ATTR_TYPE_END) break;
            if (type == 0xFFFFFFFF) break;
            
            uint32_t length = read_le32((const uint8_t *)&attr->length);
            if (length < 24 || attr_offset + length > record_size) break;
            
            if (type == ATTR_TYPE_DATA && !attr->non_resident && attr->name_length == 0) {
                const attr_resident_t *res = (const attr_resident_t *)attr;
                uint16_t value_offset = read_le16((const uint8_t *)&res->value_offset);
                uint32_t value_length = read_le32((const uint8_t *)&res->value_length);
                
                if (offset + size <= value_length) {
                    memcpy(buf, record + attr_offset + value_offset + offset, size);
                    free(record);
                    return SALVAGE_OK;
                }
                break;
            }
            
            attr_offset += length;
        }
        
        free(record);
        LOG_ERROR("Resident data not found or too small");
        return SALVAGE_ERR_IO;
    }
    
    // Non-resident data: parse data runs
    // Re-read MFT record to get data runs
    uint8_t *record = malloc(vol->mft_record_size);
    if (!record) return SALVAGE_ERR_NO_MEMORY;
    
    int ret = ntfs_read_mft_record(vol, info->mft_number, record);
    if (ret != SALVAGE_OK) {
        free(record);
        return ret;
    }
    
    // Find $DATA attribute
    const mft_record_header_t *header = (const mft_record_header_t *)record;
    uint32_t attr_offset = read_le16((const uint8_t *)&header->attrs_offset);
    uint32_t record_size = read_le32((const uint8_t *)&header->bytes_allocated);
    
    const attr_non_resident_t *nr_attr = NULL;
    
    while (attr_offset + 4 <= record_size) {
        const attr_header_t *attr = (const attr_header_t *)(record + attr_offset);
        uint32_t type = read_le32((const uint8_t *)&attr->type);
        
        if (type == ATTR_TYPE_END) break;
        if (type == 0xFFFFFFFF) break;
        
        uint32_t length = read_le32((const uint8_t *)&attr->length);
        if (length < 24 || attr_offset + length > record_size) break;
        
        if (type == ATTR_TYPE_DATA && attr->non_resident && attr->name_length == 0) {
            nr_attr = (const attr_non_resident_t *)attr;
            break;
        }
        
        attr_offset += length;
    }
    
    if (!nr_attr) {
        free(record);
        LOG_ERROR("Non-resident $DATA attribute not found");
        return SALVAGE_ERR_IO;
    }
    
    // Parse data runs
    uint16_t runs_offset = read_le16((const uint8_t *)&nr_attr->data_runs_offset);
    const uint8_t *runs = record + attr_offset + runs_offset;
    
    uint64_t current_vcn = 0;
    uint64_t current_lcn = 0;
    uint64_t data_offset = 0;
    uint64_t bytes_remaining = size;
    uint8_t *out_ptr = (uint8_t *)buf;
    
    const uint8_t *run_ptr = runs;
    
    while (*run_ptr != 0 && bytes_remaining > 0) {
        // Parse run header
        uint8_t header = *run_ptr++;
        uint8_t size_bytes = header & 0x0F;
        uint8_t offset_bytes = (header >> 4) & 0x0F;
        
        if (size_bytes == 0) break;
        
        // Read run length
        uint64_t run_length = 0;
        for (int i = 0; i < size_bytes; i++) {
            run_length |= (uint64_t)(*run_ptr++) << (i * 8);
        }
        
        // Read run offset (signed)
        int64_t run_offset = 0;
        for (int i = 0; i < offset_bytes; i++) {
            run_offset |= (int64_t)(*run_ptr++) << (i * 8);
        }
        // Sign extend if needed
        if (offset_bytes > 0 && (run_offset >> (offset_bytes * 8 - 1) & 1)) {
            for (int i = offset_bytes; i < 8; i++) {
                run_offset |= (int64_t)0xFF << (i * 8);
            }
        }
        
        current_lcn += run_offset;
        uint64_t run_bytes = run_length * vol->cluster_size;
        
        // Check if this run contains data we need
        if (data_offset + run_bytes > offset) {
            // Calculate how much to read from this run
            uint64_t run_start = (offset > data_offset) ? offset - data_offset : 0;
            uint64_t run_read = run_bytes - run_start;
            if (run_read > bytes_remaining) run_read = bytes_remaining;
            
            // Read from disk
            uint64_t read_lba = vol->partition_start + current_lcn * vol->sectors_per_cluster;
            uint64_t read_offset_bytes = run_start;
            uint64_t read_sectors = (run_read + vol->bytes_per_sector - 1) / vol->bytes_per_sector;
            
            // Allocate temporary buffer for sector-aligned read
            uint8_t *tmp = malloc(read_sectors * vol->bytes_per_sector);
            if (!tmp) {
                free(record);
                return SALVAGE_ERR_NO_MEMORY;
            }
            
            ret = device_read_sectors(vol->device, read_lba, (uint32_t)read_sectors, tmp);
            if (ret != SALVAGE_OK) {
                free(tmp);
                free(record);
                return ret;
            }
            
            memcpy(out_ptr, tmp + read_offset_bytes, run_read);
            free(tmp);
            
            out_ptr += run_read;
            bytes_remaining -= run_read;
            data_offset += run_bytes;
        } else {
            data_offset += run_bytes;
        }
        
        current_vcn += run_length;
    }
    
    free(record);
    
    if (bytes_remaining > 0) {
        // Fill remaining bytes with zeros (sparse)
        memset(out_ptr, 0, bytes_remaining);
    }
    
    return SALVAGE_OK;
}
