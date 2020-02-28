#ifndef KNIT_FRAME_DARRAY_H
#define KNIT_FRAME_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_frame" by a typename*/
/*                    "knit_frame_darray" by a proper prefix*/
/*                    "KNIT_FRAME_DARRAY" by a proper prefix*/

typedef struct knit_frame knit_frame_darray_elem_type;

struct knit_frame_darray {
    knit_frame_darray_elem_type *data;
    int len;
    int cap;
};
enum KNIT_FRAME_DARRAY_DEF {
    KNIT_FRAME_DARRAY_OK,
    KNIT_FRAME_DARRAY_NOMEM,
    KNIT_FRAME_DARRAY_RUNTIME_ERR,
};
static void knit_frame_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int knit_frame_darray_deinit(struct knit_frame_darray *knit_frame_darray); //fwd
static int knit_frame_darray_set_cap(struct knit_frame_darray *knit_frame_darray, int cap) {
    if (!cap) {
        return knit_frame_darray_deinit(knit_frame_darray);
    }
    knit_frame_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(knit_frame_darray->data, cap * sizeof(knit_frame_darray_elem_type));
    if (!new_mem) {
        return KNIT_FRAME_DARRAY_NOMEM;
    }
    knit_frame_darray->data = new_mem;
    knit_frame_darray->cap = cap;
    return KNIT_FRAME_DARRAY_OK;
}
static int knit_frame_darray_init(struct knit_frame_darray *knit_frame_darray, int cap) {
    knit_frame_darray->data = NULL;
    knit_frame_darray->len = 0;
    knit_frame_darray->cap = 0;
    return knit_frame_darray_set_cap(knit_frame_darray, cap);
}
static int knit_frame_darray_deinit(struct knit_frame_darray *knit_frame_darray) {
    free(knit_frame_darray->data);
    knit_frame_darray->data = NULL;
    knit_frame_darray->len = 0;
    knit_frame_darray->cap = 0;
    return KNIT_FRAME_DARRAY_OK;
}
static int knit_frame_darray_push(struct knit_frame_darray *knit_frame_darray, knit_frame_darray_elem_type *elem) {
    int rv;
    if (knit_frame_darray->len + 1 > knit_frame_darray->cap) {
        if (!knit_frame_darray->cap)
            knit_frame_darray->cap = 1;
        rv = knit_frame_darray_set_cap(knit_frame_darray, knit_frame_darray->cap * 2);
        if (rv != KNIT_FRAME_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(knit_frame_darray->data + knit_frame_darray->len, elem, sizeof *elem);
    knit_frame_darray->len++;
    return KNIT_FRAME_DARRAY_OK;
}
#endif //KNIT_FRAME_DARRAY_H
