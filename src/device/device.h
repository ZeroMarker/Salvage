#ifndef SALVAGE_DEVICE_H
#define SALVAGE_DEVICE_H

#include <salvage/salvage.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_PHYSICAL,    // Physical disk
    DEVICE_TYPE_PARTITION,   // Logical partition
    DEVICE_TYPE_FILE,        // Image file
} device_type_t;

typedef struct device {
    char name[256];           // Device name/path
    char description[256];    // Human-readable description
    device_type_t type;
    uint64_t total_sectors;   // Total sectors
    uint32_t sector_size;     // Bytes per sector (512 or 4096)
    uint64_t size_bytes;      // Total size in bytes
    int is_readonly;          // Opened as read-only
    void *handle;             // Platform-specific handle
    void *private_data;       // Platform-specific data
} device_t;

// Open device for reading
int device_open(device_t *dev, const char *path);

// Close device
void device_close(device_t *dev);

// Read sectors from device
int device_read_sectors(device_t *dev, uint64_t lba, uint32_t count, void *buf);

// Read bytes from device
int device_read_bytes(device_t *dev, uint64_t offset, uint64_t size, void *buf);

// Get device info string
void device_get_info(const device_t *dev, char *buf, size_t buf_size);

#endif // SALVAGE_DEVICE_H
