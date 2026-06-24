#include "cmd.h"
#include "device/device.h"
#include "partition/partition.h"
#include "core/scanner.h"
#include "core/recover.h"
#include "utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(void) {
    printf("Recover a deleted file\n\n");
    printf("Usage: salvage recover <device> <file_id> [options]\n\n");
    printf("Options:\n");
    printf("  -o, --output <path>   Output directory (default: ./recovered)\n");
    printf("  -p, --partition <n>   Partition index (default: auto)\n");
    printf("  -m, --mode <mode>     Scan mode for finding file (default: quick)\n");
    printf("  -h, --help            Show this help\n");
    printf("  -v, --verbose         Verbose output\n\n");
    printf("Example:\n");
    printf("  salvage scan \\\\.\\PhysicalDrive0\n");
    printf("  salvage recover \\\\.\\PhysicalDrive0 12345 -o D:\\recovered\n");
}

static void recover_progress(uint64_t written, uint64_t total, void *user_data) {
    (void)user_data;
    int percent = total > 0 ? (int)((written * 100) / total) : 0;
    fprintf(stderr, "\r  [");
    int width = 30;
    int filled = (percent * width) / 100;
    for (int i = 0; i < width; i++) {
        fprintf(stderr, i < filled ? "█" : "░");
    }
    fprintf(stderr, "] %3d%%", percent);
    fflush(stderr);
}

int cmd_recover(int argc, char *argv[]) {
    const char *device_path = NULL;
    const char *output_dir = "./recovered";
    uint64_t file_id = 0;
    int partition_index = -1;
    scan_mode_t mode = SCAN_QUICK;
    int has_file_id = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_dir = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--partition") == 0) {
            if (i + 1 < argc) partition_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                i++;
                if (strcmp(argv[i], "quick") == 0) mode = SCAN_QUICK;
                else if (strcmp(argv[i], "deep") == 0) mode = SCAN_DEEP;
                else if (strcmp(argv[i], "signature") == 0) mode = SCAN_SIGNATURE;
            }
        } else if (argv[i][0] != '-') {
            if (!device_path) {
                device_path = argv[i];
            } else if (!has_file_id) {
                file_id = strtoull(argv[i], NULL, 10);
                has_file_id = 1;
            }
        }
    }
    
    if (!device_path || !has_file_id) {
        fprintf(stderr, "Error: Device path and file ID required\n");
        print_help();
        return 1;
    }
    
    // Open device
    device_t dev;
    int ret = device_open(&dev, device_path);
    if (ret != SALVAGE_OK) {
        fprintf(stderr, "Error: Cannot open device %s\n", device_path);
        return 1;
    }
    
    // Detect partitions
    partition_table_t table;
    ret = pt_detect(&dev, &table);
    if (ret != SALVAGE_OK || table.count == 0) {
        fprintf(stderr, "Error: No partitions found\n");
        device_close(&dev);
        return 1;
    }
    
    // Select partition
    if (partition_index < 0) {
        for (int i = 0; i < table.count; i++) {
            if (table.entries[i].fs_type == FS_TYPE_NTFS) {
                partition_index = i;
                break;
            }
        }
        if (partition_index < 0) partition_index = 0;
    }
    
    partition_t *part = &table.entries[partition_index];
    
    // First, scan to find the file
    printf("Scanning for file ID %llu...\n", (unsigned long long)file_id);
    
    scan_task_t *scan = scan_create(&dev, part, mode);
    if (!scan) {
        fprintf(stderr, "Error: Failed to create scan task\n");
        device_close(&dev);
        return 1;
    }
    
    ret = scan_start(scan);
    if (ret != SALVAGE_OK) {
        fprintf(stderr, "Warning: Scan completed with errors\n");
    }
    
    // Find the file in results
    scan_result_t *results;
    int result_count;
    scan_get_results(scan, &results, &result_count);
    
    scan_result_t *target = NULL;
    for (int i = 0; i < result_count; i++) {
        if (results[i].file_id == file_id) {
            target = &results[i];
            break;
        }
    }
    
    if (!target) {
        fprintf(stderr, "Error: File ID %llu not found\n", (unsigned long long)file_id);
        scan_destroy(scan);
        device_close(&dev);
        return 1;
    }
    
    printf("Found: %s (%llu bytes)\n", target->name, (unsigned long long)target->size);
    printf("Recovering to: %s/%s\n\n", output_dir, target->name);
    
    // Create recovery task
    recover_task_t *recover = recover_create(&dev, target, output_dir);
    if (!recover) {
        fprintf(stderr, "Error: Failed to create recovery task\n");
        scan_destroy(scan);
        device_close(&dev);
        return 1;
    }
    
    recover_set_progress(recover, recover_progress, NULL);
    
    ret = recover_start(recover);
    fprintf(stderr, "\n");
    
    if (ret == SALVAGE_OK) {
        printf("Recovery complete: %s/%s\n", output_dir, target->name);
    } else {
        fprintf(stderr, "Error: Recovery failed (code %d)\n", ret);
    }
    
    recover_destroy(recover);
    scan_destroy(scan);
    device_close(&dev);
    
    return (ret == SALVAGE_OK) ? 0 : 1;
}
