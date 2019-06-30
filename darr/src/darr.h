#ifndef DARR_H
#define DARR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "_DELEM_TYPE_" by a typename*/
/*                    "darr" by a proper prefix*/
/*                    "DARR" by a proper prefix*/

typedef _DELEM_TYPE_ delem_type;

struct darr {
    delem_type *data;
    int len;
    int cap;
};
enum DARR_DEF {
    DARR_OK,
    DARR_NOMEM,
    DARR_RUNTIME_ERR,
};
static void darr_assert(int condition, const char *msg) {
    assert(condition);
}
static int darr_deinit(struct darr *darr); //fwd
static int darr_set_cap(struct darr *darr, int cap) {
    if (!cap) {
        return darr_deinit(darr);
    }
    darr_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(darr->data, cap * sizeof(delem_type));
    if (!new_mem) {
        return DARR_NOMEM;
    }
    darr->data = new_mem;
    darr->cap = cap;
    return DARR_OK;
}
static int darr_init(struct darr *darr, int cap) {
    darr->data = NULL;
    darr->len = 0;
    darr->cap = 0;
    return darr_set_cap(darr, cap);
}
static int darr_deinit(struct darr *darr) {
    free(darr->data);
    darr->data = NULL;
    darr->len = 0;
    darr->cap = 0;
    return DARR_OK;
}
static int darr_push(struct darr *darr, delem_type *elem) {
    int rv;
    if (darr->len + 1 > darr->cap) {
        if (!darr->cap)
            darr->cap = 1;
        rv = darr_set_cap(darr, darr->cap * 2);
        if (rv != DARR_OK) {
            return rv;
        }
    }
    memcpy(darr->data + darr->len, elem, sizeof *elem);
    darr->len++;
    return DARR_OK;
}
#endif //DARR_H
