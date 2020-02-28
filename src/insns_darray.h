#ifndef INSNS_DARRAY_H
#define INSNS_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_insn" by a typename*/
/*                    "insns_darray" by a proper prefix*/
/*                    "INSNS_DARRAY" by a proper prefix*/

typedef struct knit_insn insns_darray_elem_type;

struct insns_darray {
    insns_darray_elem_type *data;
    int len;
    int cap;
};
enum INSNS_DARRAY_DEF {
    INSNS_DARRAY_OK,
    INSNS_DARRAY_NOMEM,
    INSNS_DARRAY_RUNTIME_ERR,
};
static void insns_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int insns_darray_deinit(struct insns_darray *insns_darray); //fwd
static int insns_darray_set_cap(struct insns_darray *insns_darray, int cap) {
    if (!cap) {
        return insns_darray_deinit(insns_darray);
    }
    insns_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(insns_darray->data, cap * sizeof(insns_darray_elem_type));
    if (!new_mem) {
        return INSNS_DARRAY_NOMEM;
    }
    insns_darray->data = new_mem;
    insns_darray->cap = cap;
    return INSNS_DARRAY_OK;
}
static int insns_darray_init(struct insns_darray *insns_darray, int cap) {
    insns_darray->data = NULL;
    insns_darray->len = 0;
    insns_darray->cap = 0;
    return insns_darray_set_cap(insns_darray, cap);
}
static int insns_darray_deinit(struct insns_darray *insns_darray) {
    free(insns_darray->data);
    insns_darray->data = NULL;
    insns_darray->len = 0;
    insns_darray->cap = 0;
    return INSNS_DARRAY_OK;
}
static int insns_darray_push(struct insns_darray *insns_darray, insns_darray_elem_type *elem) {
    int rv;
    if (insns_darray->len + 1 > insns_darray->cap) {
        if (!insns_darray->cap)
            insns_darray->cap = 1;
        rv = insns_darray_set_cap(insns_darray, insns_darray->cap * 2);
        if (rv != INSNS_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(insns_darray->data + insns_darray->len, elem, sizeof *elem);
    insns_darray->len++;
    return INSNS_DARRAY_OK;
}
#endif //INSNS_DARRAY_H
