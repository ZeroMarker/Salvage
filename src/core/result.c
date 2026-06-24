#include "result.h"
#include "utils/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void result_list_init(result_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int result_list_add(result_list_t *list, const scan_result_t *result) {
    if (!list || !result) return SALVAGE_ERR_INVALID;
    
    // Grow if needed
    if (list->count >= list->capacity) {
        int new_cap = list->capacity ? list->capacity * 2 : 256;
        scan_result_t *new_items = realloc(list->items, new_cap * sizeof(scan_result_t));
        if (!new_items) return SALVAGE_ERR_NO_MEMORY;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    memcpy(&list->items[list->count], result, sizeof(scan_result_t));
    list->count++;
    
    return SALVAGE_OK;
}

static int compare_result(const void *a, const void *b) {
    const scan_result_t *ra = (const scan_result_t *)a;
    const scan_result_t *rb = (const scan_result_t *)b;
    
    if (ra->file_id < rb->file_id) return -1;
    if (ra->file_id > rb->file_id) return 1;
    return 0;
}

void result_list_sort(result_list_t *list) {
    if (!list || list->count <= 1) return;
    qsort(list->items, list->count, sizeof(scan_result_t), compare_result);
}

void result_list_free(result_list_t *list) {
    if (list) {
        free(list->items);
        list->items = NULL;
        list->count = 0;
        list->capacity = 0;
    }
}

int result_list_export_json(const result_list_t *list, const char *path) {
    if (!list || !path) return SALVAGE_ERR_INVALID;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("Cannot create output file: %s", path);
        return SALVAGE_ERR_IO;
    }
    
    fprintf(f, "{\n  \"results\": [\n");
    
    for (int i = 0; i < list->count; i++) {
        const scan_result_t *r = &list->items[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"id\": %llu,\n", (unsigned long long)r->file_id);
        fprintf(f, "      \"name\": \"%s\",\n", r->name);
        fprintf(f, "      \"extension\": \"%s\",\n", r->extension);
        fprintf(f, "      \"size\": %llu,\n", (unsigned long long)r->size);
        fprintf(f, "      \"deleted\": %s,\n", r->is_deleted ? "true" : "false");
        fprintf(f, "      \"confidence\": %.1f,\n", r->confidence);
        fprintf(f, "      \"category\": \"%s\"\n", sig_category_name(r->category));
        fprintf(f, "    }%s\n", i < list->count - 1 ? "," : "");
    }
    
    fprintf(f, "  ],\n  \"count\": %d\n}\n", list->count);
    fclose(f);
    
    LOG_INFO("Exported %d results to %s", list->count, path);
    return SALVAGE_OK;
}
