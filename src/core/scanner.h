#ifndef SALVAGE_SCANNER_H
#define SALVAGE_SCANNER_H

#include <salvage/salvage.h>
#include "device/device.h"
#include "partition/partition.h"
#include "fs/ntfs/ntfs.h"
#include "signature/signature.h"
#include <stdint.h>

// Scan mode
typedef enum {
    SCAN_QUICK = 0,      // MFT scan only
    SCAN_DEEP,           // MFT + signature scan
    SCAN_SIGNATURE       // Signature scan only
} scan_mode_t;

// Scan result entry
typedef struct {
    uint64_t file_id;        // MFT number or sector offset
    char name[256];          // File name
    char extension[16];      // File extension
    uint64_t size;           // File size in bytes
    uint64_t create_time;    // Creation time (NTFS)
    uint64_t modify_time;    // Modification time (NTFS)
    uint64_t data_offset;    // Data offset on disk
    int is_deleted;          // Is deleted file
    int is_directory;        // Is directory
    float confidence;        // Recovery confidence (0-100)
    sig_category_t category; // File category
    char signature_name[32]; // Signature match name
} scan_result_t;

// Scan task
typedef struct scan_task scan_task_t;

// Progress callback
typedef void (*scan_progress_cb)(int percent, int files_found, void *user_data);

// Create scan task
scan_task_t* scan_create(device_t *dev, partition_t *partition, scan_mode_t mode);

// Set signature database
void scan_set_signatures(scan_task_t *task, signature_db_t *db);

// Set progress callback
void scan_set_progress(scan_task_t *task, scan_progress_cb cb, void *user_data);

// Start scanning (blocking)
int scan_start(scan_task_t *task);

// Cancel scan
void scan_cancel(scan_task_t *task);

// Get results
int scan_get_results(scan_task_t *task, scan_result_t **results, int *count);

// Destroy scan task
void scan_destroy(scan_task_t *task);

#endif // SALVAGE_SCANNER_H
