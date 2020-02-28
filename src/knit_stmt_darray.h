#ifndef KNIT_STMT_DARRAY_H
#define KNIT_STMT_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_stmt *" by a typename*/
/*                    "knit_stmt_darray" by a proper prefix*/
/*                    "KNIT_STMT_DARRAY" by a proper prefix*/

typedef struct knit_stmt * knit_stmt_darray_elem_type;

struct knit_stmt_darray {
    knit_stmt_darray_elem_type *data;
    int len;
    int cap;
};
enum KNIT_STMT_DARRAY_DEF {
    KNIT_STMT_DARRAY_OK,
    KNIT_STMT_DARRAY_NOMEM,
    KNIT_STMT_DARRAY_RUNTIME_ERR,
};
static void knit_stmt_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int knit_stmt_darray_deinit(struct knit_stmt_darray *knit_stmt_darray); //fwd
static int knit_stmt_darray_set_cap(struct knit_stmt_darray *knit_stmt_darray, int cap) {
    if (!cap) {
        return knit_stmt_darray_deinit(knit_stmt_darray);
    }
    knit_stmt_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(knit_stmt_darray->data, cap * sizeof(knit_stmt_darray_elem_type));
    if (!new_mem) {
        return KNIT_STMT_DARRAY_NOMEM;
    }
    knit_stmt_darray->data = new_mem;
    knit_stmt_darray->cap = cap;
    return KNIT_STMT_DARRAY_OK;
}
static int knit_stmt_darray_init(struct knit_stmt_darray *knit_stmt_darray, int cap) {
    knit_stmt_darray->data = NULL;
    knit_stmt_darray->len = 0;
    knit_stmt_darray->cap = 0;
    return knit_stmt_darray_set_cap(knit_stmt_darray, cap);
}
static int knit_stmt_darray_deinit(struct knit_stmt_darray *knit_stmt_darray) {
    free(knit_stmt_darray->data);
    knit_stmt_darray->data = NULL;
    knit_stmt_darray->len = 0;
    knit_stmt_darray->cap = 0;
    return KNIT_STMT_DARRAY_OK;
}
static int knit_stmt_darray_push(struct knit_stmt_darray *knit_stmt_darray, knit_stmt_darray_elem_type *elem) {
    int rv;
    if (knit_stmt_darray->len + 1 > knit_stmt_darray->cap) {
        if (!knit_stmt_darray->cap)
            knit_stmt_darray->cap = 1;
        rv = knit_stmt_darray_set_cap(knit_stmt_darray, knit_stmt_darray->cap * 2);
        if (rv != KNIT_STMT_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(knit_stmt_darray->data + knit_stmt_darray->len, elem, sizeof *elem);
    knit_stmt_darray->len++;
    return KNIT_STMT_DARRAY_OK;
}
#endif //KNIT_STMT_DARRAY_H
