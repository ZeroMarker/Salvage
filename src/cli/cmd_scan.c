#include "cmd.h"
#include "device/device.h"
#include "partition/partition.h"
#include "core/scanner.h"
#include "core/result.h"
#include "utils/log.h"
#include "utils/str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(void) {
    printf("Scan for deleted files\n\n");
    printf("Usage: salvage scan <device> [options]\n\n");
    printf("Options:\n");
    printf("  -m, --mode <quick|deep|signature>  Scan mode (default: quick)\n");
    printf("  -p, --partition <index>            Partition index (default: auto)\n");
    printf("  -o, --output <path>                Output JSON file\n");
    printf("  -h, --help                         Show this help\n");
    printf("  -v, --verbose                      Verbose output\n");
}

static void progress_callback(int percent, int files_found, void *user_data) {
    (void)user_data;
    fprintf(stderr, "\r  [");
    int width = 30;
    int filled = (percent * width) / 100;
    for (int i = 0; i < width; i++) {
        fprintf(stderr, i < filled ? "█" : "░");
    }
    fprintf(stderr, "] %3d%% - Found %d files", percent, files_found);
    fflush(stderr);
}

int cmd_scan(int argc, char *argv[]) {
    const char *device_path = NULL;
    const char *output_path = NULL;
    scan_mode_t mode = SCAN_QUICK;
    int partition_index = -1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                i++;
                if (strcmp(argv[i], "quick") == 0) mode = SCAN_QUICK;
                else if (strcmp(argv[i], "deep") == 0) mode = SCAN_DEEP;
                else if (strcmp(argv[i], "signature") == 0) mode = SCAN_SIGNATURE;
                else {
                    fprintf(stderr, "Error: Unknown mode '%s'\n", argv[i]);
                    return 1;
                }
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--partition") == 0) {
            if (i + 1 < argc) {
                partition_index = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            }
        } else if (argv[i][0] != '-') {
            device_path = argv[i];
        }
    }
    
    if (!device_path) {
        fprintf(stderr, "Error: Device path required\n");
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
        // Auto-select first NTFS partition
        for (int i = 0; i < table.count; i++) {
            if (table.entries[i].fs_type == FS_TYPE_NTFS) {
                partition_index = i;
                break;
            }
        }
        if (partition_index < 0) partition_index = 0;
    }
    
    if (partition_index >= table.count) {
        fprintf(stderr, "Error: Partition index %d out of range (0-%d)\n",
                partition_index, table.count - 1);
        device_close(&dev);
        return 1;
    }
    
    partition_t *part = &table.entries[partition_index];
    
    printf("Scanning %s...\n", device_path);
    printf("  Partition %d: %s (%llu bytes) [%s]\n\n",
           partition_index,
           fs_type_name(part->fs_type),
           (unsigned long long)part->size_bytes,
           part->name);
    
    // Create scan task
    scan_task_t *task = scan_create(&dev, part, mode);
    if (!task) {
        fprintf(stderr, "Error: Failed to create scan task\n");
        device_close(&dev);
        return 1;
    }
    
    scan_set_progress(task, progress_callback, NULL);
    
    // Run scan
    ret = scan_start(task);
    fprintf(stderr, "\n\n");
    
    if (ret != SALVAGE_OK && ret != SALVAGE_ERR_CANCELLED) {
        fprintf(stderr, "Warning: Scan completed with errors\n");
    }
    
    // Get results
    scan_result_t *results;
    int result_count;
    scan_get_results(task, &results, &result_count);
    
    if (result_count == 0) {
        printf("No deleted files found.\n");
    } else {
        printf("Found %d deleted files:\n\n", result_count);
        printf("  %-6s %-30s %-12s %-10s %s\n",
               "ID", "Name", "Size", "Type", "Confidence");
        printf("  %-6s %-30s %-12s %-10s %s\n",
               "--", "----", "----", "----", "----------");
        
        for (int i = 0; i < result_count && i < 100; i++) {
            scan_result_t *r = &results[i];
            char size_str[32];
            format_size(r->size, size_str, sizeof(size_str));
            
            printf("  %-6llu %-30.30s %-12s %-10s %.0f%%\n",
                   (unsigned long long)r->file_id,
                   r->name,
                   size_str,
                   r->signature_name[0] ? r->signature_name : "MFT",
                   r->confidence);
        }
        
        if (result_count > 100) {
            printf("  ... and %d more files\n", result_count - 100);
        }
    }
    
    // Export if requested
    if (output_path) {
        result_list_t list = { .items = results, .count = result_count };
        result_list_export_json(&list, output_path);
        printf("\nResults exported to: %s\n", output_path);
    }
    
    scan_destroy(task);
    device_close(&dev);
    
    return 0;
}
