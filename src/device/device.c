#include "device.h"
#include "utils/log.h"
#include <string.h>
#include <stdio.h>

// Platform-specific implementations
extern int device_open_platform(device_t *dev, const char *path);
extern void device_close_platform(device_t *dev);
extern int device_read_platform(device_t *dev, uint64_t offset, uint64_t size, void *buf);

int device_open(device_t *dev, const char *path) {
    if (!dev || !path) return SALVAGE_ERR_INVALID;
    
    memset(dev, 0, sizeof(device_t));
    strncpy(dev->name, path, sizeof(dev->name) - 1);
    dev->is_readonly = 1;
    
    int ret = device_open_platform(dev, path);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to open device: %s", path);
        return ret;
    }
    
    LOG_INFO("Opened device: %s (%llu sectors, %u bytes/sector)",
             dev->name, (unsigned long long)dev->total_sectors, dev->sector_size);
    
    return SALVAGE_OK;
}

void device_close(device_t *dev) {
    if (!dev || !dev->handle) return;
    device_close_platform(dev);
    dev->handle = NULL;
    LOG_DEBUG("Closed device: %s", dev->name);
}

int device_read_sectors(device_t *dev, uint64_t lba, uint32_t count, void *buf) {
    if (!dev || !buf || count == 0) return SALVAGE_ERR_INVALID;
    
    uint64_t offset = lba * dev->sector_size;
    uint64_t size = (uint64_t)count * dev->sector_size;
    
    // Bounds check
    if (lba + count > dev->total_sectors) {
        LOG_ERROR("Read beyond device bounds: LBA %llu + %u > %llu",
                  (unsigned long long)lba, count, (unsigned long long)dev->total_sectors);
        return SALVAGE_ERR_IO;
    }
    
    return device_read_platform(dev, offset, size, buf);
}

int device_read_bytes(device_t *dev, uint64_t offset, uint64_t size, void *buf) {
    if (!dev || !buf || size == 0) return SALVAGE_ERR_INVALID;
    
    // Bounds check
    if (offset + size > dev->size_bytes) {
        LOG_ERROR("Read beyond device bounds: offset %llu + %llu > %llu",
                  (unsigned long long)offset, (unsigned long long)size,
                  (unsigned long long)dev->size_bytes);
        return SALVAGE_ERR_IO;
    }
    
    return device_read_platform(dev, offset, size, buf);
}

void device_get_info(const device_t *dev, char *buf, size_t buf_size) {
    if (!dev || !buf) return;
    
    const char *type_str;
    switch (dev->type) {
        case DEVICE_TYPE_PHYSICAL:  type_str = "Physical"; break;
        case DEVICE_TYPE_PARTITION: type_str = "Partition"; break;
        case DEVICE_TYPE_FILE:      type_str = "Image"; break;
        default:                    type_str = "Unknown"; break;
    }
    
    // Format size
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = (double)dev->size_bytes;
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    snprintf(buf, buf_size, "%s %s (%.1f %s, %u B/sector)",
             type_str, dev->name, size, units[unit], dev->sector_size);
}
