#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd.h"
#include "utils/log.h"

static void print_version(void) {
    printf("Salvage v0.3.0 - Data Recovery Tool\n");
}

static void print_usage(void) {
    printf("Usage: salvage <command> [options]\n\n");
    printf("Commands:\n");
    printf("  list      List disks and partitions\n");
    printf("  scan      Scan for deleted files\n");
    printf("  recover   Recover a file\n");
    printf("  help      Show this help\n\n");
    printf("Run 'salvage <command> --help' for command-specific help.\n");
}

int main(int argc, char *argv[]) {
    // Set default log level
    log_set_level(LOG_LEVEL_INFO);
    
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    // Check for verbose flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            log_set_level(LOG_LEVEL_DEBUG);
            break;
        }
    }
    
    const char *cmd = argv[1];
    
    if (strcmp(cmd, "list") == 0) {
        return cmd_list(argc - 1, argv + 1);
    } else if (strcmp(cmd, "scan") == 0) {
        return cmd_scan(argc - 1, argv + 1);
    } else if (strcmp(cmd, "recover") == 0) {
        return cmd_recover(argc - 1, argv + 1);
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        if (argc > 2) {
            // Help for specific command
            char subcmd[32];
            snprintf(subcmd, sizeof(subcmd), "%s", argv[2]);
            char *help_argv[] = {subcmd, "--help", NULL};
            if (strcmp(subcmd, "list") == 0) return cmd_list(2, help_argv);
            if (strcmp(subcmd, "scan") == 0) return cmd_scan(2, help_argv);
            if (strcmp(subcmd, "recover") == 0) return cmd_recover(2, help_argv);
        }
        print_version();
        print_usage();
        return 0;
    } else if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-V") == 0) {
        print_version();
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return 1;
    }
    
    return 0;
}
