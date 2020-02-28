#ifndef KNIT_OBJP_DARRAY_H
#define KNIT_OBJP_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_obj *" by a typename*/
/*                    "knit_objp_darray" by a proper prefix*/
/*                    "KNIT_OBJP_DARRAY" by a proper prefix*/

typedef struct knit_obj * knit_objp_darray_elem_type;

struct knit_objp_darray {
    knit_objp_darray_elem_type *data;
    int len;
    int cap;
};
enum KNIT_OBJP_DARRAY_DEF {
    KNIT_OBJP_DARRAY_OK,
    KNIT_OBJP_DARRAY_NOMEM,
    KNIT_OBJP_DARRAY_RUNTIME_ERR,
};
static void knit_objp_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int knit_objp_darray_deinit(struct knit_objp_darray *knit_objp_darray); //fwd
static int knit_objp_darray_set_cap(struct knit_objp_darray *knit_objp_darray, int cap) {
    if (!cap) {
        return knit_objp_darray_deinit(knit_objp_darray);
    }
    knit_objp_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(knit_objp_darray->data, cap * sizeof(knit_objp_darray_elem_type));
    if (!new_mem) {
        return KNIT_OBJP_DARRAY_NOMEM;
    }
    knit_objp_darray->data = new_mem;
    knit_objp_darray->cap = cap;
    return KNIT_OBJP_DARRAY_OK;
}
static int knit_objp_darray_init(struct knit_objp_darray *knit_objp_darray, int cap) {
    knit_objp_darray->data = NULL;
    knit_objp_darray->len = 0;
    knit_objp_darray->cap = 0;
    return knit_objp_darray_set_cap(knit_objp_darray, cap);
}
static int knit_objp_darray_deinit(struct knit_objp_darray *knit_objp_darray) {
    free(knit_objp_darray->data);
    knit_objp_darray->data = NULL;
    knit_objp_darray->len = 0;
    knit_objp_darray->cap = 0;
    return KNIT_OBJP_DARRAY_OK;
}
static int knit_objp_darray_push(struct knit_objp_darray *knit_objp_darray, knit_objp_darray_elem_type *elem) {
    int rv;
    if (knit_objp_darray->len + 1 > knit_objp_darray->cap) {
        if (!knit_objp_darray->cap)
            knit_objp_darray->cap = 1;
        rv = knit_objp_darray_set_cap(knit_objp_darray, knit_objp_darray->cap * 2);
        if (rv != KNIT_OBJP_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(knit_objp_darray->data + knit_objp_darray->len, elem, sizeof *elem);
    knit_objp_darray->len++;
    return KNIT_OBJP_DARRAY_OK;
}
#endif //KNIT_OBJP_DARRAY_H
