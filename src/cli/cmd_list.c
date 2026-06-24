#include "cmd.h"
#include "device/device.h"
#include "partition/partition.h"
#include "utils/log.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

static void print_help(void) {
    printf("List disks and partitions\n\n");
    printf("Usage: salvage list [options]\n\n");
    printf("Options:\n");
    printf("  -h, --help    Show this help\n");
    printf("  -v, --verbose Verbose output\n");
}

#ifdef PLATFORM_WINDOWS
static void list_physical_disks(void) {
    printf("Physical Disks:\n");
    printf("  %-20s %-12s %-10s %s\n", "Device", "Size", "Sector", "Type");
    printf("  %-20s %-12s %-10s %s\n", "------", "----", "------", "----");
    
    for (int i = 0; i < 16; i++) {
        char path[64];
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);
        
        device_t dev;
        if (device_open(&dev, path) == SALVAGE_OK) {
            char info[256];
            device_get_info(&dev, info, sizeof(info));
            printf("  %-20s %-12llu %-10u Physical\n", 
                   path, (unsigned long long)dev.size_bytes, dev.sector_size);
            device_close(&dev);
        }
    }
    printf("\n");
}

static void list_partitions(void) {
    printf("Partitions:\n");
    printf("  %-10s %-8s %-12s %-8s %s\n", "Letter", "Type", "Size", "Sector", "Label");
    printf("  %-10s %-8s %-12s %-8s %s\n", "------", "----", "----", "------", "-----");
    
    for (int i = 0; i < 26; i++) {
        char path[16];
        snprintf(path, sizeof(path), "\\\\.\\%c:", 'A' + i);
        
        device_t dev;
        if (device_open(&dev, path) == SALVAGE_OK) {
            char info[256];
            device_get_info(&dev, info, sizeof(info));
            printf("  %-10s %-8s %-12llu %-8u Partition\n",
                   path, "?", (unsigned long long)dev.size_bytes, dev.sector_size);
            device_close(&dev);
        }
    }
    printf("\n");
}
#endif

static void list_disk_partitions(const char *disk_path) {
    device_t dev;
    int ret = device_open(&dev, disk_path);
    if (ret != SALVAGE_OK) {
        fprintf(stderr, "Error: Cannot open %s\n", disk_path);
        return;
    }
    
    partition_table_t table;
    ret = pt_detect(&dev, &table);
    if (ret != SALVAGE_OK) {
        printf("No partition table found on %s\n", disk_path);
        device_close(&dev);
        return;
    }
    
    printf("Partition table on %s (%s):\n\n", disk_path,
           table.type == PT_TYPE_GPT ? "GPT" : "MBR");
    
    printf("  %-5s %-10s %-15s %-12s %-10s %s\n",
           "ID", "Start LBA", "Size", "Type", "Boot", "Name");
    printf("  %-5s %-10s %-15s %-12s %-10s %s\n",
           "--", "---------", "----", "----", "----", "----");
    
    for (int i = 0; i < table.count; i++) {
        partition_t *p = &table.entries[i];
        printf("  %-5d %-10llu %-15llu %-12s %-10s %s\n",
               i,
               (unsigned long long)p->start_lba,
               (unsigned long long)p->size_bytes,
               fs_type_name(p->fs_type),
               p->is_bootable ? "Yes" : "-",
               p->name);
    }
    
    device_close(&dev);
}

int cmd_list(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        }
    }
    
#ifdef PLATFORM_WINDOWS
    list_physical_disks();
    list_partitions();
#else
    printf("On Linux, specify disk path directly:\n");
    printf("  salvage list /dev/sda\n\n");
#endif
    
    // If a disk path is provided, list its partitions
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            list_disk_partitions(argv[i]);
        }
    }
    
    return 0;
}
