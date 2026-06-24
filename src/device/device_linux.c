#ifdef PLATFORM_LINUX

#include "device.h"
#include "utils/log.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

typedef struct {
    int fd;
} device_linux_data_t;

int device_open_platform(device_t *dev, const char *path) {
    device_linux_data_t *data = calloc(1, sizeof(device_linux_data_t));
    if (!data) return SALVAGE_ERR_NO_MEMORY;
    
    data->fd = open(path, O_RDONLY);
    if (data->fd < 0) {
        LOG_ERROR("open failed for %s: %s", path, strerror(errno));
        free(data);
        return (errno == EACCES) ? SALVAGE_ERR_PERMISSION : SALVAGE_ERR_IO;
    }
    
    dev->handle = (void *)(intptr_t)data->fd;
    dev->private_data = data;
    
    // Determine device type
    struct stat st;
    if (fstat(data->fd, &st) == 0) {
        if (S_ISBLK(st.st_mode)) {
            dev->type = DEVICE_TYPE_PHYSICAL;
        } else if (S_ISREG(st.st_mode)) {
            dev->type = DEVICE_TYPE_FILE;
        } else {
            dev->type = DEVICE_TYPE_UNKNOWN;
        }
    }
    
    // Get device size
    uint64_t size = 0;
    if (dev->type == DEVICE_TYPE_PHYSICAL) {
        if (ioctl(data->fd, BLKGETSIZE64, &size) == 0) {
            dev->size_bytes = size;
        }
        // Get sector size
        int sector_size = 512;
        if (ioctl(data->fd, BLKSSZGET, &sector_size) == 0) {
            dev->sector_size = (uint32_t)sector_size;
        } else {
            dev->sector_size = 512;
        }
    } else {
        // Regular file
        dev->size_bytes = st.st_size;
        dev->sector_size = 512;
    }
    
    dev->total_sectors = dev->size_bytes / dev->sector_size;
    
    return SALVAGE_OK;
}

void device_close_platform(device_t *dev) {
    device_linux_data_t *data = (device_linux_data_t *)dev->private_data;
    if (data) {
        if (data->fd >= 0) {
            close(data->fd);
        }
        free(data);
        dev->private_data = NULL;
    }
}

int device_read_platform(device_t *dev, uint64_t offset, uint64_t size, void *buf) {
    device_linux_data_t *data = (device_linux_data_t *)dev->private_data;
    
    ssize_t result = pread(data->fd, buf, size, offset);
    if (result < 0) {
        LOG_ERROR("pread failed at offset %llu: %s", offset, strerror(errno));
        return SALVAGE_ERR_IO;
    }
    if ((uint64_t)result < size) {
        LOG_ERROR("Short read at offset %llu: got %zd, expected %llu",
                  offset, result, size);
        return SALVAGE_ERR_IO;
    }
    
    return SALVAGE_OK;
}

#endif // PLATFORM_LINUX
