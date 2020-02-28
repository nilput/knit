#ifndef KNIT_VARNAME_DARRAY_H
#define KNIT_VARNAME_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_varname" by a typename*/
/*                    "knit_varname_darray" by a proper prefix*/
/*                    "KNIT_VARNAME_DARRAY" by a proper prefix*/

typedef struct knit_varname knit_varname_darray_elem_type;

struct knit_varname_darray {
    knit_varname_darray_elem_type *data;
    int len;
    int cap;
};
enum KNIT_VARNAME_DARRAY_DEF {
    KNIT_VARNAME_DARRAY_OK,
    KNIT_VARNAME_DARRAY_NOMEM,
    KNIT_VARNAME_DARRAY_RUNTIME_ERR,
};
static void knit_varname_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int knit_varname_darray_deinit(struct knit_varname_darray *knit_varname_darray); //fwd
static int knit_varname_darray_set_cap(struct knit_varname_darray *knit_varname_darray, int cap) {
    if (!cap) {
        return knit_varname_darray_deinit(knit_varname_darray);
    }
    knit_varname_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(knit_varname_darray->data, cap * sizeof(knit_varname_darray_elem_type));
    if (!new_mem) {
        return KNIT_VARNAME_DARRAY_NOMEM;
    }
    knit_varname_darray->data = new_mem;
    knit_varname_darray->cap = cap;
    return KNIT_VARNAME_DARRAY_OK;
}
static int knit_varname_darray_init(struct knit_varname_darray *knit_varname_darray, int cap) {
    knit_varname_darray->data = NULL;
    knit_varname_darray->len = 0;
    knit_varname_darray->cap = 0;
    return knit_varname_darray_set_cap(knit_varname_darray, cap);
}
static int knit_varname_darray_deinit(struct knit_varname_darray *knit_varname_darray) {
    free(knit_varname_darray->data);
    knit_varname_darray->data = NULL;
    knit_varname_darray->len = 0;
    knit_varname_darray->cap = 0;
    return KNIT_VARNAME_DARRAY_OK;
}
static int knit_varname_darray_push(struct knit_varname_darray *knit_varname_darray, knit_varname_darray_elem_type *elem) {
    int rv;
    if (knit_varname_darray->len + 1 > knit_varname_darray->cap) {
        if (!knit_varname_darray->cap)
            knit_varname_darray->cap = 1;
        rv = knit_varname_darray_set_cap(knit_varname_darray, knit_varname_darray->cap * 2);
        if (rv != KNIT_VARNAME_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(knit_varname_darray->data + knit_varname_darray->len, elem, sizeof *elem);
    knit_varname_darray->len++;
    return KNIT_VARNAME_DARRAY_OK;
}
#endif //KNIT_VARNAME_DARRAY_H
