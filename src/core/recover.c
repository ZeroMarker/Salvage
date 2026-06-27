#include "recover.h"
#include "fs/ntfs/ntfs.h"
#include "fs/fat/fat.h"
#include "fs/exfat/exfat.h"
#include "partition/partition.h"
#include "utils/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#define mkdir(path) CreateDirectoryA(path, NULL)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#endif

struct recover_task {
    device_t *device;
    scan_result_t result;
    char output_path[512];
    uint64_t partition_start_lba;
    
    recover_progress_cb progress_cb;
    void *user_data;
    
    volatile int cancelled;
};

recover_task_t* recover_create(device_t *dev, const scan_result_t *result, const char *output_dir, uint64_t partition_start_lba) {
    if (!dev || !result || !output_dir) return NULL;
    
    recover_task_t *task = calloc(1, sizeof(recover_task_t));
    if (!task) return NULL;
    
    task->device = dev;
    memcpy(&task->result, result, sizeof(scan_result_t));
    task->partition_start_lba = partition_start_lba;
    
    // Build output path
    snprintf(task->output_path, sizeof(task->output_path), "%s/%s", output_dir, result->name);
    
    return task;
}

void recover_set_progress(recover_task_t *task, recover_progress_cb cb, void *user_data) {
    if (task) {
        task->progress_cb = cb;
        task->user_data = user_data;
    }
}

// Recover file using MFT information
static int recover_from_mft(recover_task_t *task) {
    ntfs_volume_t vol;
    int ret = ntfs_init(&vol, task->device, task->partition_start_lba);
    if (ret != SALVAGE_OK) {
        // Try using file_id as MFT number on current partition
        LOG_WARN("Cannot init NTFS volume for MFT recovery");
        return ret;
    }
    
    // Read MFT record
    uint8_t *record = malloc(vol.mft_record_size);
    if (!record) return SALVAGE_ERR_NO_MEMORY;
    
    ret = ntfs_read_mft_record(&vol, task->result.file_id, record);
    if (ret != SALVAGE_OK) {
        free(record);
        return ret;
    }
    
    // Parse file info
    ntfs_file_info_t info;
    ret = ntfs_parse_file_info(record, &info);
    free(record);
    
    if (ret != SALVAGE_OK) return ret;
    
    if (info.is_resident) {
        // Resident data - read from MFT
        uint8_t *data = malloc(info.data_size);
        if (!data) return SALVAGE_ERR_NO_MEMORY;
        
        ret = ntfs_read_file_data(&vol, &info, 0, info.data_size, data);
        if (ret != SALVAGE_OK) {
            free(data);
            return ret;
        }
        
        // Write to file
        FILE *f = fopen(task->output_path, "wb");
        if (!f) {
            free(data);
            LOG_ERROR("Cannot create output file: %s", task->output_path);
            return SALVAGE_ERR_IO;
        }
        
        fwrite(data, 1, info.data_size, f);
        fclose(f);
        free(data);
        
        if (task->progress_cb) {
            task->progress_cb(info.data_size, info.data_size, task->user_data);
        }
    } else {
        // Non-resident data - read data runs
        uint64_t total_size = info.data_size;
        uint64_t chunk_size = 65536;  // 64KB chunks
        uint8_t *chunk = malloc(chunk_size);
        if (!chunk) return SALVAGE_ERR_NO_MEMORY;
        
        FILE *f = fopen(task->output_path, "wb");
        if (!f) {
            free(chunk);
            LOG_ERROR("Cannot create output file: %s", task->output_path);
            return SALVAGE_ERR_IO;
        }
        
        uint64_t offset = 0;
        while (offset < total_size && !task->cancelled) {
            uint64_t to_read = chunk_size;
            if (offset + to_read > total_size) to_read = total_size - offset;
            
            ret = ntfs_read_file_data(&vol, &info, offset, to_read, chunk);
            if (ret != SALVAGE_OK) {
                LOG_WARN("Read error at offset %llu", offset);
                // Fill with zeros on error
                memset(chunk, 0, to_read);
            }
            
            fwrite(chunk, 1, to_read, f);
            offset += to_read;
            
            if (task->progress_cb) {
                task->progress_cb(offset, total_size, task->user_data);
            }
        }
        
        fclose(f);
        free(chunk);
    }
    
    return task->cancelled ? SALVAGE_ERR_CANCELLED : SALVAGE_OK;
}

// Recover file using signature (raw carve)
static int recover_from_signature(recover_task_t *task) {
    uint64_t offset = task->result.data_offset;
    uint64_t size = task->result.size;
    
    if (size == 0 || size > 1073741824) {  // Max 1GB
        LOG_ERROR("Invalid file size for signature recovery: %llu", size);
        return SALVAGE_ERR_INVALID;
    }
    
    uint8_t *buf = malloc(size);
    if (!buf) return SALVAGE_ERR_NO_MEMORY;
    
    int ret = device_read_bytes(task->device, offset, size, buf);
    if (ret != SALVAGE_OK) {
        free(buf);
        return ret;
    }
    
    FILE *f = fopen(task->output_path, "wb");
    if (!f) {
        free(buf);
        LOG_ERROR("Cannot create output file: %s", task->output_path);
        return SALVAGE_ERR_IO;
    }
    
    fwrite(buf, 1, size, f);
    fclose(f);
    free(buf);
    
    if (task->progress_cb) {
        task->progress_cb(size, size, task->user_data);
    }
    
    return SALVAGE_OK;
}

// Recover file using FAT32 cluster chain
static int recover_from_fat(recover_task_t *task) {
    fat_volume_t vol;
    int ret = fat_init(&vol, task->device, task->partition_start_lba);
    if (ret != SALVAGE_OK) {
        LOG_WARN("Cannot init FAT32 volume for recovery");
        return ret;
    }

    fat_file_info_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, task->result.name, sizeof(info.name) - 1);
    info.first_cluster = task->result.first_cluster;
    info.file_size = task->result.size;

    if (info.first_cluster < 2) {
        LOG_ERROR("Invalid first cluster for FAT recovery");
        return SALVAGE_ERR_INVALID;
    }

    uint64_t total_size = info.file_size;
    uint64_t chunk_size = 65536;
    uint8_t *chunk = malloc(chunk_size);
    if (!chunk) return SALVAGE_ERR_NO_MEMORY;

    FILE *f = fopen(task->output_path, "wb");
    if (!f) {
        free(chunk);
        LOG_ERROR("Cannot create output file: %s", task->output_path);
        return SALVAGE_ERR_IO;
    }

    uint64_t offset = 0;
    while (offset < total_size && !task->cancelled) {
        uint64_t to_read = chunk_size;
        if (offset + to_read > total_size) to_read = total_size - offset;

        ret = fat_read_file_data(&vol, &info, offset, to_read, chunk);
        if (ret != SALVAGE_OK) {
            memset(chunk, 0, to_read);
        }

        fwrite(chunk, 1, to_read, f);
        offset += to_read;

        if (task->progress_cb) {
            task->progress_cb(offset, total_size, task->user_data);
        }
    }

    fclose(f);
    free(chunk);
    return task->cancelled ? SALVAGE_ERR_CANCELLED : SALVAGE_OK;
}

// Recover file using exFAT cluster chain
static int recover_from_exfat(recover_task_t *task) {
    exfat_volume_t vol;
    int ret = exfat_init(&vol, task->device, task->partition_start_lba);
    if (ret != SALVAGE_OK) {
        LOG_WARN("Cannot init exFAT volume for recovery");
        return ret;
    }

    exfat_file_info_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, task->result.name, sizeof(info.name) - 1);
    info.first_cluster = task->result.first_cluster;
    info.valid_data_length = task->result.size;
    info.data_length = task->result.size;

    if (info.first_cluster < 2) {
        LOG_ERROR("Invalid first cluster for exFAT recovery");
        return SALVAGE_ERR_INVALID;
    }

    uint64_t total_size = info.valid_data_length;
    uint64_t chunk_size = 65536;
    uint8_t *chunk = malloc(chunk_size);
    if (!chunk) return SALVAGE_ERR_NO_MEMORY;

    FILE *f = fopen(task->output_path, "wb");
    if (!f) {
        free(chunk);
        LOG_ERROR("Cannot create output file: %s", task->output_path);
        return SALVAGE_ERR_IO;
    }

    uint64_t offset = 0;
    while (offset < total_size && !task->cancelled) {
        uint64_t to_read = chunk_size;
        if (offset + to_read > total_size) to_read = total_size - offset;

        ret = exfat_read_file_data(&vol, &info, offset, to_read, chunk);
        if (ret != SALVAGE_OK) {
            memset(chunk, 0, to_read);
        }

        fwrite(chunk, 1, to_read, f);
        offset += to_read;

        if (task->progress_cb) {
            task->progress_cb(offset, total_size, task->user_data);
        }
    }

    fclose(f);
    free(chunk);
    return task->cancelled ? SALVAGE_ERR_CANCELLED : SALVAGE_OK;
}

int recover_start(recover_task_t *task) {
    if (!task) return SALVAGE_ERR_INVALID;
    
    LOG_INFO("Recovering file: %s (%llu bytes) -> %s",
             task->result.name, (unsigned long long)task->result.size, task->output_path);
    
    // Create output directory if needed
    char *last_sep = strrchr(task->output_path, '/');
    if (!last_sep) last_sep = strrchr(task->output_path, '\\');
    if (last_sep) {
        char dir[512];
        strncpy(dir, task->output_path, last_sep - task->output_path);
        dir[last_sep - task->output_path] = '\0';
#ifdef PLATFORM_WINDOWS
        mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
    }
    
    // Choose recovery method
    if (task->result.signature_name[0] != '\0') {
        return recover_from_signature(task);
    } else if (task->result.fs_type == FS_TYPE_FAT32) {
        return recover_from_fat(task);
    } else if (task->result.fs_type == FS_TYPE_EXFAT) {
        return recover_from_exfat(task);
    } else {
        return recover_from_mft(task);
    }
}

void recover_cancel(recover_task_t *task) {
    if (task) task->cancelled = 1;
}

void recover_destroy(recover_task_t *task) {
    if (task) free(task);
}
