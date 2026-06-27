#include "cmd.h"
#include "device/device.h"
#include "partition/partition.h"
#include "core/scanner.h"
#include "core/result.h"
#include "signature/signature.h"
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
    printf("  -t, --type <category>              Filter by type: image, doc, audio, video, archive, all (default: all)\n");
    printf("  -s, --min-size <bytes>             Minimum file size in bytes\n");
    printf("  -o, --output <path>                Output JSON to file\n");
    printf("  -j, --json                         Output JSON to stdout\n");
    printf("  -h, --help                         Show this help\n");
    printf("  -v, --verbose                      Verbose output\n");
}

static sig_category_t parse_type_filter(const char *type) {
    if (strcmp(type, "image") == 0) return SIG_CATEGORY_IMAGE;
    if (strcmp(type, "doc") == 0 || strcmp(type, "document") == 0) return SIG_CATEGORY_DOCUMENT;
    if (strcmp(type, "audio") == 0) return SIG_CATEGORY_AUDIO;
    if (strcmp(type, "video") == 0) return SIG_CATEGORY_VIDEO;
    if (strcmp(type, "archive") == 0) return SIG_CATEGORY_ARCHIVE;
    if (strcmp(type, "executable") == 0) return SIG_CATEGORY_EXECUTABLE;
    return SIG_CATEGORY_UNKNOWN;
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
    int type_filter = -1;  // -1 = all
    uint64_t min_size = 0;
    int json_stdout = 0;
    int verbose = 0;
    
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
            if (i + 1 < argc) partition_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
            if (i + 1 < argc) {
                i++;
                if (strcmp(argv[i], "all") == 0) {
                    type_filter = -1;
                } else {
                    type_filter = (int)parse_type_filter(argv[i]);
                    if (type_filter == (int)SIG_CATEGORY_UNKNOWN) {
                        fprintf(stderr, "Error: Unknown type '%s'\n", argv[i]);
                        return 1;
                    }
                }
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--min-size") == 0) {
            if (i + 1 < argc) min_size = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_path = argv[++i];
        } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--json") == 0) {
            json_stdout = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (argv[i][0] != '-') {
            device_path = argv[i];
        }
    }
    
    if (verbose) log_set_level(LOG_LEVEL_DEBUG);
    
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
        // No partition table — treat device as a whole volume
        memset(&table, 0, sizeof(table));
        table.type = PT_TYPE_NONE;
        table.count = 1;
        table.device = &dev;
        table.entries[0].start_lba = 0;
        table.entries[0].size_sectors = dev.total_sectors;
        table.entries[0].size_bytes = dev.size_bytes;
        table.entries[0].index = 0;

        // Detect filesystem from boot sector
        uint8_t boot[512];
        if (device_read_sectors(&dev, 0, 1, boot) == SALVAGE_OK) {
            if (memcmp(boot + 3, "NTFS    ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_NTFS;
            else if (memcmp(boot + 3, "FAT32   ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_FAT32;
            else if (memcmp(boot + 3, "EXFAT   ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_EXFAT;
            else if (memcmp(boot + 3, "FAT16   ", 8) == 0 ||
                     memcmp(boot + 3, "FAT12   ", 8) == 0)
                table.entries[0].fs_type = FS_TYPE_FAT16;
        }

        LOG_INFO("No partition table, treating as volume (fs=%s)",
                 fs_type_name(table.entries[0].fs_type));
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
    
    // Apply filters
    result_list_t list = { .items = results, .count = result_count };
    
    if (type_filter >= 0) {
        result_list_filter_by_category(&list, type_filter);
    }
    if (min_size > 0) {
        result_list_filter_by_min_size(&list, min_size);
    }
    
    int filtered_count = list.count;
    
    if (json_stdout) {
        result_list_print_json(&list);
    } else if (filtered_count == 0) {
        printf("No deleted files found.\n");
    } else {
        printf("Found %d deleted files:\n\n", filtered_count);
        printf("  %-6s %-30s %-12s %-10s %s\n",
               "ID", "Name", "Size", "Type", "Confidence");
        printf("  %-6s %-30s %-12s %-10s %s\n",
               "--", "----", "----", "----", "----------");
        
        for (int i = 0; i < filtered_count && i < 100; i++) {
            scan_result_t *r = &list.items[i];
            char size_str[32];
            format_size(r->size, size_str, sizeof(size_str));
            
            printf("  %-6llu %-30.30s %-12s %-10s %.0f%%\n",
                   (unsigned long long)r->file_id,
                   r->name,
                   size_str,
                   r->signature_name[0] ? r->signature_name : "MFT",
                   r->confidence);
        }
        
        if (filtered_count > 100) {
            printf("  ... and %d more files\n", filtered_count - 100);
        }
    }
    
    // Export to file if requested
    if (output_path) {
        result_list_t export_list = { .items = results, .count = result_count };
        if (type_filter >= 0) result_list_filter_by_category(&export_list, type_filter);
        if (min_size > 0) result_list_filter_by_min_size(&export_list, min_size);
        result_list_export_json(&export_list, output_path);
        if (!json_stdout) printf("\nResults exported to: %s\n", output_path);
    }
    
    scan_destroy(task);
    device_close(&dev);
    
    return 0;
}
