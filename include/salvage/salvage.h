#ifndef SALVAGE_H
#define SALVAGE_H

#define SALVAGE_VERSION_MAJOR 0
#define SALVAGE_VERSION_MINOR 1
#define SALVAGE_VERSION_PATCH 0
#define SALVAGE_VERSION "0.1.0"

#include <stdint.h>
#include <stddef.h>

// Error codes
typedef enum {
    SALVAGE_OK              =  0,
    SALVAGE_ERR_IO          = -1,
    SALVAGE_ERR_INVALID     = -2,
    SALVAGE_ERR_NOT_FOUND   = -3,
    SALVAGE_ERR_NO_MEMORY   = -4,
    SALVAGE_ERR_PERMISSION  = -5,
    SALVAGE_ERR_NOT_SUPPORTED = -6,
    SALVAGE_ERR_CORRUPT     = -7,
    SALVAGE_ERR_CANCELLED   = -8,
} salvage_error_t;

// Sector size constants
#define SECTOR_SIZE_512     512
#define SECTOR_SIZE_4096    4096
#define DEFAULT_SECTOR_SIZE SECTOR_SIZE_512

#endif // SALVAGE_H
