#ifdef PLATFORM_WINDOWS

#include "device.h"
#include "utils/log.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    HANDLE handle;
} device_win_data_t;

int device_open_platform(device_t *dev, const char *path) {
    device_win_data_t *data = calloc(1, sizeof(device_win_data_t));
    if (!data) return SALVAGE_ERR_NO_MEMORY;
    
    // Determine device type from path
    if (strncmp(path, "\\\\.\\PhysicalDrive", 17) == 0) {
        dev->type = DEVICE_TYPE_PHYSICAL;
    } else if (strncmp(path, "\\\\.\\", 4) == 0 && path[5] == ':') {
        dev->type = DEVICE_TYPE_PARTITION;
    } else {
        dev->type = DEVICE_TYPE_FILE;
    }
    
    // Open handle
    DWORD access = GENERIC_READ;
    DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD flags = FILE_FLAG_NO_BUFFERING;
    
    data->handle = CreateFileA(path, access, share, NULL, OPEN_EXISTING, flags, NULL);
    if (data->handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LOG_ERROR("CreateFile failed for %s: error %lu", path, err);
        free(data);
        return (err == ERROR_ACCESS_DENIED) ? SALVAGE_ERR_PERMISSION : SALVAGE_ERR_IO;
    }
    
    dev->handle = data->handle;
    dev->private_data = data;
    
    // Get disk geometry
    if (dev->type == DEVICE_TYPE_PHYSICAL) {
        DISK_GEOMETRY_EX geom;
        DWORD bytes_returned;
        if (DeviceIoControl(data->handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                           NULL, 0, &geom, sizeof(geom), &bytes_returned, NULL)) {
            dev->sector_size = geom.Geometry.BytesPerSector;
            dev->total_sectors = geom.DiskSize.QuadPart / dev->sector_size;
            dev->size_bytes = geom.DiskSize.QuadPart;
        } else {
            dev->sector_size = 512;
        }
    } else if (dev->type == DEVICE_TYPE_PARTITION) {
        // Get partition size
        PARTITION_INFORMATION_EX pi;
        DWORD bytes_returned;
        if (DeviceIoControl(data->handle, IOCTL_DISK_GET_PARTITION_INFO_EX,
                           NULL, 0, &pi, sizeof(pi), &bytes_returned, NULL)) {
            dev->size_bytes = pi.PartitionLength.QuadPart;
            dev->sector_size = 512;
            dev->total_sectors = dev->size_bytes / dev->sector_size;
        }
        
        // Get volume cluster size
        DWORD sectors_per_cluster, bytes_per_sector;
        GetDiskFreeSpaceA(path, &sectors_per_cluster, &bytes_per_sector,
                         NULL, NULL);
        dev->sector_size = bytes_per_sector;
    } else {
        // Image file
        LARGE_INTEGER file_size;
        GetFileSizeEx(data->handle, &file_size);
        dev->size_bytes = file_size.QuadPart;
        dev->sector_size = 512;
        dev->total_sectors = dev->size_bytes / dev->sector_size;
    }
    
    if (dev->sector_size == 0) dev->sector_size = 512;
    if (dev->size_bytes > 0 && dev->total_sectors == 0) {
        dev->total_sectors = dev->size_bytes / dev->sector_size;
    }
    
    return SALVAGE_OK;
}

void device_close_platform(device_t *dev) {
    device_win_data_t *data = (device_win_data_t *)dev->private_data;
    if (data) {
        if (data->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(data->handle);
        }
        free(data);
        dev->private_data = NULL;
    }
}

int device_read_platform(device_t *dev, uint64_t offset, uint64_t size, void *buf) {
    device_win_data_t *data = (device_win_data_t *)dev->private_data;
    
    // Seek to position
    LARGE_INTEGER li;
    li.QuadPart = offset;
    if (!SetFilePointerEx(data->handle, li, NULL, FILE_BEGIN)) {
        LOG_ERROR("Seek failed at offset %llu", offset);
        return SALVAGE_ERR_IO;
    }
    
    // Read data in chunks (Windows has DWORD size limit)
    uint8_t *ptr = (uint8_t *)buf;
    uint64_t remaining = size;
    
    while (remaining > 0) {
        DWORD to_read = (remaining > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)remaining;
        DWORD bytes_read = 0;
        
        if (!ReadFile(data->handle, ptr, to_read, &bytes_read, NULL)) {
            LOG_ERROR("ReadFile failed at offset %llu", offset);
            return SALVAGE_ERR_IO;
        }
        
        if (bytes_read == 0) {
            LOG_ERROR("Unexpected end of data at offset %llu", offset);
            return SALVAGE_ERR_IO;
        }
        
        ptr += bytes_read;
        remaining -= bytes_read;
        offset += bytes_read;
    }
    
    return SALVAGE_OK;
}

#endif // PLATFORM_WINDOWS
