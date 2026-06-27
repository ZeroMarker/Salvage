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

// Export results to JSON file
int result_list_export_json(const result_list_t *list, const char *path);

// Print results as JSON to stdout
void result_list_print_json(const result_list_t *list);

// Filter results by category (in-place, returns new count)
int result_list_filter_by_category(result_list_t *list, int category);

// Filter results by minimum size (in-place, returns new count)
int result_list_filter_by_min_size(result_list_t *list, uint64_t min_size);

#endif // SALVAGE_RESULT_H
