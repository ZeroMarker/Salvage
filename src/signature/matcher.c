#include "signature.h"
#include "utils/log.h"
#include <string.h>

// Empty implementation - matching is in database.c
int sig_match_header_simple(const signature_db_t *db, const uint8_t *data, int len) {
    return sig_match_header(db, data, len);
}
