#include "cmd.h"
#include "device/device.h"
#include "partition/partition.h"
#include "core/scanner.h"
#include "core/recover.h"
#include "utils/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECOVER_IDS 256

static void print_help(void) {
    printf("Recover deleted files\n\n");
    printf("Usage: salvage recover <device> <file_ids> [options]\n\n");
    printf("Options:\n");
    printf("  -o, --output <path>   Output directory (default: ./recovered)\n");
    printf("  -p, --partition <n>   Partition index (default: auto)\n");
    printf("  -m, --mode <mode>     Scan mode for finding file (default: quick)\n");
    printf("  -f, --force           Overwrite existing files\n");
    printf("  -h, --help            Show this help\n");
    printf("  -v, --verbose         Verbose output\n\n");
    printf("File IDs:\n");
    printf("  Single ID:     12345\n");
    printf("  Multiple IDs:  1,2,3,5\n");
    printf("  Range:         1-10\n\n");
    printf("Example:\n");
    printf("  salvage recover \\\\.\\PhysicalDrive0 12345 -o D:\\recovered\n");
    printf("  salvage recover \\\\.\\PhysicalDrive0 1,5,10 -o D:\\recovered\n");
    printf("  salvage recover \\\\.\\PhysicalDrive0 100-200 -f\n");
}

static int parse_file_ids(const char *arg, uint64_t *ids, int max_ids) {
    int count = 0;
    const char *p = arg;
    
    while (*p && count < max_ids) {
        char *end;
        uint64_t id = strtoull(p, &end, 10);
        if (end == p) break;
        
        if (*end == '-') {
            // Range: id-end
            p = end + 1;
            uint64_t id_end = strtoull(p, &end, 10);
            if (end == p) break;
            for (uint64_t r = id; r <= id_end && count < max_ids; r++) {
                ids[count++] = r;
            }
            p = end;
        } else {
            ids[count++] = id;
            p = end;
        }
        
        if (*p == ',') p++;
    }
    
    return count;
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

static void scan_progress(int percent, int files_found, void *user_data) {
    (void)user_data;
    fprintf(stderr, "\r  Scanning... [");
    int width = 30;
    int filled = (percent * width) / 100;
    for (int i = 0; i < width; i++) {
        fprintf(stderr, i < filled ? "█" : "░");
    }
    fprintf(stderr, "] %3d%% - %d files", percent, files_found);
    fflush(stderr);
}

int cmd_recover(int argc, char *argv[]) {
    const char *device_path = NULL;
    const char *output_dir = "./recovered";
    const char *ids_arg = NULL;
    int partition_index = -1;
    scan_mode_t mode = SCAN_QUICK;
    int force = 0;
    int verbose = 0;
    
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
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            force = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            if (!device_path) {
                device_path = argv[i];
            } else if (!ids_arg) {
                ids_arg = argv[i];
            }
        }
    }
    
    if (verbose) log_set_level(LOG_LEVEL_DEBUG);
    
    if (!device_path || !ids_arg) {
        fprintf(stderr, "Error: Device path and file ID(s) required\n");
        print_help();
        return 1;
    }
    
    uint64_t file_ids[MAX_RECOVER_IDS];
    int id_count = parse_file_ids(ids_arg, file_ids, MAX_RECOVER_IDS);
    if (id_count == 0) {
        fprintf(stderr, "Error: No valid file IDs specified\n");
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
        memset(&table, 0, sizeof(table));
        table.type = PT_TYPE_NONE;
        table.count = 1;
        table.device = &dev;
        table.entries[0].start_lba = 0;
        table.entries[0].size_sectors = dev.total_sectors;
        table.entries[0].size_bytes = dev.size_bytes;
        table.entries[0].index = 0;

        uint8_t boot[512];
        if (device_read_sectors(&dev, 0, 1, boot) == SALVAGE_OK) {
            if (memcmp(boot + 3, "NTFS    ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_NTFS;
            else if (memcmp(boot + 3, "FAT32   ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_FAT32;
            else if (memcmp(boot + 3, "EXFAT   ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_EXFAT;
        }

        LOG_INFO("No partition table, treating as volume (fs=%s)",
                 fs_type_name(table.entries[0].fs_type));
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
    
    // Scan to find files
    printf("Scanning for %d file(s)...\n", id_count);
    
    scan_task_t *scan = scan_create(&dev, part, mode);
    if (!scan) {
        fprintf(stderr, "Error: Failed to create scan task\n");
        device_close(&dev);
        return 1;
    }
    
    scan_set_progress(scan, scan_progress, NULL);
    
    ret = scan_start(scan);
    fprintf(stderr, "\n");
    if (ret != SALVAGE_OK) {
        fprintf(stderr, "Warning: Scan completed with errors\n");
    }
    
    // Get all scan results
    scan_result_t *results;
    int result_count;
    scan_get_results(scan, &results, &result_count);
    
    // Recover each requested file
    int recovered = 0;
    int failed = 0;
    
    for (int n = 0; n < id_count; n++) {
        uint64_t file_id = file_ids[n];
        
        // Find the file in results
        scan_result_t *target = NULL;
        for (int i = 0; i < result_count; i++) {
            if (results[i].file_id == file_id) {
                target = &results[i];
                break;
            }
        }
        
        if (!target) {
            fprintf(stderr, "Error: File ID %llu not found\n", (unsigned long long)file_id);
            failed++;
            continue;
        }
        
        printf("[%d/%d] %s (%llu bytes)\n", n + 1, id_count, target->name, (unsigned long long)target->size);
        
        // Check if output file already exists
        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, target->name);
        if (!force) {
            FILE *check = fopen(out_path, "rb");
            if (check) {
                fclose(check);
                fprintf(stderr, "  Skipped: file exists (use -f to overwrite)\n");
                failed++;
                continue;
            }
        }
        
        // Create recovery task
        recover_task_t *recover = recover_create(&dev, target, output_dir, part->start_lba);
        if (!recover) {
            fprintf(stderr, "  Error: Failed to create recovery task\n");
            failed++;
            continue;
        }
        
        recover_set_progress(recover, recover_progress, NULL);
        
        ret = recover_start(recover);
        fprintf(stderr, "\n");
        
        if (ret == SALVAGE_OK) {
            printf("  -> %s/%s\n", output_dir, target->name);
            recovered++;
        } else {
            fprintf(stderr, "  Error: Recovery failed (code %d)\n", ret);
            failed++;
        }
        
        recover_destroy(recover);
    }
    
    printf("\nRecovery complete: %d succeeded, %d failed\n", recovered, failed);
    
    scan_destroy(scan);
    device_close(&dev);
    
    return (failed == 0) ? 0 : 1;
}
