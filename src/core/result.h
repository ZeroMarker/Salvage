#ifndef SALVAGE_RESULT_H
#define SALVAGE_RESULT_H

#include "scanner.h"

// Result list management
typedef struct result_list {
    scan_result_t *items;
    int count;
    int capacity;
} result_list_t;

// Initialize result list
void result_list_init(result_list_t *list);

// Add result to list
int result_list_add(result_list_t *list, const scan_result_t *result);

// Sort results by file_id
void result_list_sort(result_list_t *list);

// Free result list
void result_list_free(result_list_t *list);

// Export results to JSON
int result_list_export_json(const result_list_t *list, const char *path);

#endif // SALVAGE_RESULT_H
