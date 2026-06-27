#include "scanner.h"
#include "result.h"
#include "utils/log.h"
#include "utils/time.h"
#include "utils/str.h"
#include "utils/progress.h"
#include "fs/fat/fat.h"
#include "fs/exfat/exfat.h"
#include <stdlib.h>
#include <string.h>

struct scan_task {
    device_t *device;
    partition_t partition;
    scan_mode_t mode;
    signature_db_t *sig_db;
    signature_db_t default_sig_db;
    
    result_list_t results;
    
    scan_progress_cb progress_cb;
    void *user_data;
    
    volatile int cancelled;
    ntfs_volume_t ntfs_vol;
};

scan_task_t* scan_create(device_t *dev, partition_t *partition, scan_mode_t mode) {
    if (!dev || !partition) return NULL;
    
    scan_task_t *task = calloc(1, sizeof(scan_task_t));
    if (!task) return NULL;
    
    task->device = dev;
    memcpy(&task->partition, partition, sizeof(partition_t));
    task->mode = mode;
    
    result_list_init(&task->results);
    
    // Load default signatures if needed
    if (mode == SCAN_DEEP || mode == SCAN_SIGNATURE) {
        sig_load_defaults(&task->default_sig_db);
        task->sig_db = &task->default_sig_db;
    }
    
    return task;
}

void scan_set_signatures(scan_task_t *task, signature_db_t *db) {
    if (task) task->sig_db = db;
}

void scan_set_progress(scan_task_t *task, scan_progress_cb cb, void *user_data) {
    if (task) {
        task->progress_cb = cb;
        task->user_data = user_data;
    }
}

// Calculate confidence score for MFT-based result
static float calc_mft_confidence(const ntfs_file_info_t *info) {
    float score = 50.0;  // Base: deleted MFT record found
    
    if (info->name[0] != '\0') score += 15.0;       // Has filename
    if (info->data_size > 0) score += 10.0;          // Has data size
    if (info->data_lcn > 0) score += 15.0;           // Has data runs (knows where data is)
    if (info->create_time > 0) score += 5.0;         // Has timestamps
    if (info->modify_time > 0) score += 5.0;
    
    if (score > 100.0) score = 100.0;
    return score;
}

// Quick scan: enumerate MFT for deleted files
static int scan_quick_mft(scan_task_t *task) {
    int ret = ntfs_init(&task->ntfs_vol, task->device, task->partition.start_lba);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to initialize NTFS volume");
        return ret;
    }
    
    // Estimate MFT records
    uint64_t mft_size = task->partition.size_bytes / 10;  // Rough estimate
    uint64_t total_records = mft_size / task->ntfs_vol.mft_record_size;
    if (total_records > 10000000) total_records = 10000000;  // Cap at 10M
    
    LOG_INFO("Scanning MFT (estimated %llu records)...", total_records);
    
    uint8_t *record = malloc(task->ntfs_vol.mft_record_size);
    if (!record) return SALVAGE_ERR_NO_MEMORY;
    
    progress_t progress;
    progress_init(&progress, total_records, 30);
    
    for (uint64_t i = MFT_ENTRY_FIRST_USER; i < total_records; i++) {
        if (task->cancelled) break;
        
        // Update progress periodically
        if (i % 1000 == 0) {
            int percent = (int)((i * 100) / total_records);
            if (task->progress_cb) {
                task->progress_cb(percent, task->results.count, task->user_data);
            }
            progress_update(&progress, i);
        }
        
        ret = ntfs_read_mft_record(&task->ntfs_vol, i, record);
        if (ret != SALVAGE_OK) continue;
        
        // Check if record is deleted (not in use but has valid data)
        if (!ntfs_is_deleted_record(record)) continue;
        
        // Parse file info
        ntfs_file_info_t info;
        ret = ntfs_parse_file_info(record, &info);
        if (ret != SALVAGE_OK) continue;
        
        // Skip if no name or zero size
        if (info.name[0] == '\0') continue;
        if (info.data_size == 0 && !(info.flags & MFT_FLAG_DIRECTORY)) continue;
        
        // Create scan result
        scan_result_t result;
        memset(&result, 0, sizeof(scan_result_t));
        
        result.file_id = i;
        strncpy(result.name, info.name, sizeof(result.name) - 1);
        result.size = info.data_size;
        result.create_time = info.create_time;
        result.modify_time = info.modify_time;
        result.is_deleted = 1;
        result.is_directory = (info.flags & MFT_FLAG_DIRECTORY) ? 1 : 0;
        result.data_offset = info.data_lcn;
        result.fs_type = FS_TYPE_NTFS;
        
        // Extract extension
        char *dot = strrchr(result.name, '.');
        if (dot) {
            strncpy(result.extension, dot + 1, sizeof(result.extension) - 1);
        }
        
        // Confidence based on data completeness
        result.confidence = calc_mft_confidence(&info);
        
        result_list_add(&task->results, &result);
    }
    
    progress_finish(&progress);
    free(record);
    
    return SALVAGE_OK;
}

// Signature scan: scan raw disk for file signatures
static int scan_signature(scan_task_t *task) {
    if (!task->sig_db) {
        LOG_WARN("No signature database loaded");
        return SALVAGE_ERR_INVALID;
    }
    
    LOG_INFO("Starting signature scan...");
    
    uint64_t partition_bytes = task->partition.size_sectors * task->device->sector_size;
    uint64_t scan_chunk = 4096;  // Read 4KB at a time
    uint64_t overlap = 32;      // Overlap for signatures crossing boundaries
    
    uint8_t *buf = malloc(scan_chunk + overlap);
    if (!buf) return SALVAGE_ERR_NO_MEMORY;
    
    progress_t progress;
    progress_init(&progress, partition_bytes, 30);
    
    uint64_t offset = 0;
    while (offset < partition_bytes && !task->cancelled) {
        // Update progress
        if (offset % (1024 * 1024) == 0) {
            int percent = (int)((offset * 100) / partition_bytes);
            if (task->progress_cb) {
                task->progress_cb(percent, task->results.count, task->user_data);
            }
            progress_update(&progress, offset);
        }
        
        // Read chunk
        uint64_t to_read = scan_chunk;
        if (offset + to_read > partition_bytes) to_read = partition_bytes - offset;
        
        int ret = device_read_bytes(task->device, 
                                    task->partition.start_lba * task->device->sector_size + offset,
                                    to_read, buf);
        if (ret != SALVAGE_OK) {
            offset += scan_chunk;
            continue;
        }
        
        // Scan for signatures
        for (uint64_t i = 0; i < to_read - 4; i++) {
            int sig_idx = sig_match_header(task->sig_db, buf + i, (int)(to_read - i));
            if (sig_idx < 0) continue;
            
            const signature_t *sig = sig_get(task->sig_db, sig_idx);
            if (!sig) continue;
            
            // Found a signature
            scan_result_t result;
            memset(&result, 0, sizeof(scan_result_t));
            
            result.file_id = offset + i;
            strncpy(result.name, sig->name, sizeof(result.name) - 1);
            strncpy(result.signature_name, sig->name, sizeof(result.signature_name) - 1);
            strncpy(result.extension, sig->extensions, sizeof(result.extension) - 1);
            result.category = sig->category;
            result.is_deleted = 1;
            
            // Base confidence for signature scan
            float conf = 40.0;
            conf += 10.0;  // Header matched
            if (sig->category != SIG_CATEGORY_UNKNOWN) conf += 10.0;  // Known category
            if (sig->max_size > 0 && sig->max_size < 1073741824) conf += 5.0;  // Reasonable max size
            
            // Try to determine size if footer exists
            if (sig->footer_len > 0) {
                for (uint64_t j = i + sig->header_len; j < to_read - sig->footer_len; j++) {
                    if (memcmp(buf + j, sig->footer, sig->footer_len) == 0) {
                        result.size = j + sig->footer_len - i;
                        conf += 25.0;  // Footer found = much higher confidence
                        break;
                    }
                }
            }
            
            if (result.size == 0) {
                result.size = sig->max_size;
                conf -= 10.0;  // No exact size, lower confidence
            }
            
            if (conf > 100.0) conf = 100.0;
            if (conf < 0.0) conf = 0.0;
            result.confidence = conf;
            
            if (result.size == 0) {
                result.size = sig->max_size;  // Use max size as estimate
            }
            
            result_list_add(&task->results, &result);
            
            // Skip past this signature
            i += sig->header_len;
        }
        
        offset += scan_chunk;
    }
    
    progress_finish(&progress);
    free(buf);
    
    return SALVAGE_OK;
}

// Quick scan FAT32 for deleted files
static void fat_result_callback(fat_file_info_t *info, void *ctx) {
    scan_task_t *task = (scan_task_t *)ctx;

    if ((info->attributes & FAT_ATTR_DIRECTORY) && info->file_size == 0) return;

    scan_result_t result;
    memset(&result, 0, sizeof(scan_result_t));

    result.file_id = info->first_cluster;
    strncpy(result.name, info->name, sizeof(result.name) - 1);
    result.size = info->file_size;
    result.is_deleted = 1;
    result.is_directory = (info->attributes & FAT_ATTR_DIRECTORY) ? 1 : 0;
    result.first_cluster = info->first_cluster;
    result.fs_type = FS_TYPE_FAT32;

    char *dot = strrchr(result.name, '.');
    if (dot) strncpy(result.extension, dot + 1, sizeof(result.extension) - 1);

    result.confidence = 60.0;
    if (info->first_cluster >= 2) result.confidence += 15.0;
    if (info->file_size > 0) result.confidence += 10.0;

    result_list_add(&task->results, &result);
}

static int scan_quick_fat(scan_task_t *task) {
    fat_volume_t vol;
    int ret = fat_init(&vol, task->device, task->partition.start_lba);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to initialize FAT32 volume");
        return ret;
    }

    LOG_INFO("Scanning FAT32 for deleted files...");
    fat_scan_deleted(&vol, fat_result_callback, task);
    return SALVAGE_OK;
}

// Quick scan exFAT for deleted files
static void exfat_result_callback(exfat_file_info_t *info, void *ctx) {
    scan_task_t *task = (scan_task_t *)ctx;

    if ((info->attributes & EXFAT_ATTR_DIRECTORY) && info->data_length == 0) return;

    scan_result_t result;
    memset(&result, 0, sizeof(scan_result_t));

    result.file_id = info->first_cluster;
    strncpy(result.name, info->name, sizeof(result.name) - 1);
    result.size = info->valid_data_length;
    result.is_deleted = 1;
    result.is_directory = (info->attributes & EXFAT_ATTR_DIRECTORY) ? 1 : 0;
    result.first_cluster = info->first_cluster;
    result.fs_type = FS_TYPE_EXFAT;

    char *dot = strrchr(result.name, '.');
    if (dot) strncpy(result.extension, dot + 1, sizeof(result.extension) - 1);

    result.confidence = 60.0;
    if (info->first_cluster >= 2) result.confidence += 15.0;
    if (info->valid_data_length > 0) result.confidence += 10.0;

    result_list_add(&task->results, &result);
}

static int scan_quick_exfat(scan_task_t *task) {
    exfat_volume_t vol;
    int ret = exfat_init(&vol, task->device, task->partition.start_lba);
    if (ret != SALVAGE_OK) {
        LOG_ERROR("Failed to initialize exFAT volume");
        return ret;
    }

    LOG_INFO("Scanning exFAT for deleted files...");
    exfat_scan_deleted(&vol, exfat_result_callback, task);
    return SALVAGE_OK;
}

int scan_start(scan_task_t *task) {
    if (!task) return SALVAGE_ERR_INVALID;
    
    int ret = SALVAGE_OK;
    
    LOG_INFO("Starting scan: mode=%d, partition=%s", task->mode, task->partition.name);
    
    if (task->mode == SCAN_QUICK || task->mode == SCAN_DEEP) {
        if (task->partition.fs_type == FS_TYPE_NTFS) {
            ret = scan_quick_mft(task);
            if (ret != SALVAGE_OK && task->mode == SCAN_QUICK) return ret;
        } else if (task->partition.fs_type == FS_TYPE_FAT32) {
            ret = scan_quick_fat(task);
            if (ret != SALVAGE_OK && task->mode == SCAN_QUICK) return ret;
        } else if (task->partition.fs_type == FS_TYPE_EXFAT) {
            ret = scan_quick_exfat(task);
            if (ret != SALVAGE_OK && task->mode == SCAN_QUICK) return ret;
        }
    }
    
    if (task->mode == SCAN_DEEP || task->mode == SCAN_SIGNATURE) {
        int ret2 = scan_signature(task);
        if (ret == SALVAGE_OK) ret = ret2;
    }
    
    result_list_sort(&task->results);
    
    LOG_INFO("Scan complete: %d files found", task->results.count);
    return ret;
}

void scan_cancel(scan_task_t *task) {
    if (task) task->cancelled = 1;
}

int scan_get_results(scan_task_t *task, scan_result_t **results, int *count) {
    if (!task || !results || !count) return SALVAGE_ERR_INVALID;
    
    *results = task->results.items;
    *count = task->results.count;
    return SALVAGE_OK;
}

void scan_destroy(scan_task_t *task) {
    if (!task) return;
    result_list_free(&task->results);
    free(task);
}
