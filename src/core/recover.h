#ifndef SALVAGE_RECOVER_H
#define SALVAGE_RECOVER_H

#include <salvage/salvage.h>
#include "device/device.h"
#include "scanner.h"
#include <stdint.h>

// Recovery task
typedef struct recover_task recover_task_t;

// Progress callback
typedef void (*recover_progress_cb)(uint64_t written, uint64_t total, void *user_data);

// Create recovery task
recover_task_t* recover_create(device_t *dev, const scan_result_t *result, const char *output_dir);

// Set progress callback
void recover_set_progress(recover_task_t *task, recover_progress_cb cb, void *user_data);

// Start recovery (blocking)
int recover_start(recover_task_t *task);

// Cancel recovery
void recover_cancel(recover_task_t *task);

// Destroy recovery task
void recover_destroy(recover_task_t *task);

#endif // SALVAGE_RECOVER_H
