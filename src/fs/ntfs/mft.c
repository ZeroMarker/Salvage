#include "ntfs.h"
#include "utils/endian.h"
#include "utils/log.h"
#include <string.h>
#include <stdlib.h>

int ntfs_read_mft_record(ntfs_volume_t *vol, uint64_t mft_number, uint8_t *record) {
    if (!vol || !record) return SALVAGE_ERR_INVALID;
    
    // Calculate byte offset of MFT record
    uint64_t record_offset = (uint64_t)vol->mft_lba * vol->bytes_per_sector +
                             mft_number * vol->mft_record_size;
    
    // Read the record
    int ret = device_read_bytes(vol->device, record_offset, vol->mft_record_size, record);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to read MFT record %llu", mft_number);
        return ret;
    }
    
    // Verify signature
    if (!ntfs_is_valid_mft_record(record)) {
        LOG_DEBUG("Invalid MFT record signature at %llu", mft_number);
        return SALVAGE_ERR_CORRUPT;
    }
    
    // Apply fixup (update sequence array)
    const mft_record_header_t *header = (const mft_record_header_t *)record;
    uint16_t update_offset = read_le16((const uint8_t *)&header->update_offset);
    uint16_t update_size = read_le16((const uint8_t *)&header->update_size);
    
    if (update_offset > 0 && update_size > 1) {
        // Update sequence array starts at update_offset
        // First 2 bytes = update sequence number
        // Next (update_size-1)*2 bytes = replacement values
        uint16_t usn = read_le16(record + update_offset);
        (void)usn;
        
        // Apply fixups to each sector
        for (int i = 1; i < update_size; i++) {
            uint16_t fixup = read_le16(record + update_offset + i * 2);
            uint32_t sector_end = i * vol->bytes_per_sector - 2;
            
            if (sector_end + 2 <= vol->mft_record_size) {
                write_le16(record + sector_end, fixup);
            }
        }
    }
    
    return SALVAGE_OK;
}
