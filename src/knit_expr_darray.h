#ifndef KNIT_EXPR_DARRAY_H
#define KNIT_EXPR_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_expr *" by a typename*/
/*                    "knit_expr_darray" by a proper prefix*/
/*                    "KNIT_EXPR_DARRAY" by a proper prefix*/

typedef struct knit_expr * knit_expr_darray_elem_type;

struct knit_expr_darray {
    knit_expr_darray_elem_type *data;
    int len;
    int cap;
};
enum KNIT_EXPR_DARRAY_DEF {
    KNIT_EXPR_DARRAY_OK,
    KNIT_EXPR_DARRAY_NOMEM,
    KNIT_EXPR_DARRAY_RUNTIME_ERR,
};
static void knit_expr_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int knit_expr_darray_deinit(struct knit_expr_darray *knit_expr_darray); //fwd
static int knit_expr_darray_set_cap(struct knit_expr_darray *knit_expr_darray, int cap) {
    if (!cap) {
        return knit_expr_darray_deinit(knit_expr_darray);
    }
    knit_expr_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(knit_expr_darray->data, cap * sizeof(knit_expr_darray_elem_type));
    if (!new_mem) {
        return KNIT_EXPR_DARRAY_NOMEM;
    }
    knit_expr_darray->data = new_mem;
    knit_expr_darray->cap = cap;
    return KNIT_EXPR_DARRAY_OK;
}
static int knit_expr_darray_init(struct knit_expr_darray *knit_expr_darray, int cap) {
    knit_expr_darray->data = NULL;
    knit_expr_darray->len = 0;
    knit_expr_darray->cap = 0;
    return knit_expr_darray_set_cap(knit_expr_darray, cap);
}
static int knit_expr_darray_deinit(struct knit_expr_darray *knit_expr_darray) {
    free(knit_expr_darray->data);
    knit_expr_darray->data = NULL;
    knit_expr_darray->len = 0;
    knit_expr_darray->cap = 0;
    return KNIT_EXPR_DARRAY_OK;
}
static int knit_expr_darray_push(struct knit_expr_darray *knit_expr_darray, knit_expr_darray_elem_type *elem) {
    int rv;
    if (knit_expr_darray->len + 1 > knit_expr_darray->cap) {
        if (!knit_expr_darray->cap)
            knit_expr_darray->cap = 1;
        rv = knit_expr_darray_set_cap(knit_expr_darray, knit_expr_darray->cap * 2);
        if (rv != KNIT_EXPR_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(knit_expr_darray->data + knit_expr_darray->len, elem, sizeof *elem);
    knit_expr_darray->len++;
    return KNIT_EXPR_DARRAY_OK;
}
#endif //KNIT_EXPR_DARRAY_H
