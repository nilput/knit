#ifndef TOK_DARRAY_H
#define TOK_DARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/*to use this replace "struct knit_tok" by a typename*/
/*                    "tok_darray" by a proper prefix*/
/*                    "TOK_DARRAY" by a proper prefix*/

typedef struct knit_tok tok_darray_elem_type;

struct tok_darray {
    tok_darray_elem_type *data;
    int len;
    int cap;
};
enum TOK_DARRAY_DEF {
    TOK_DARRAY_OK,
    TOK_DARRAY_NOMEM,
    TOK_DARRAY_RUNTIME_ERR,
};
static void tok_darray_assert(int condition, const char *msg) {
    assert(condition);
}
static int tok_darray_deinit(struct tok_darray *tok_darray); //fwd
static int tok_darray_set_cap(struct tok_darray *tok_darray, int cap) {
    if (!cap) {
        return tok_darray_deinit(tok_darray);
    }
    tok_darray_assert(cap > 0, "invalid capacity requested");
    void *new_mem = realloc(tok_darray->data, cap * sizeof(tok_darray_elem_type));
    if (!new_mem) {
        return TOK_DARRAY_NOMEM;
    }
    tok_darray->data = new_mem;
    tok_darray->cap = cap;
    return TOK_DARRAY_OK;
}
static int tok_darray_init(struct tok_darray *tok_darray, int cap) {
    tok_darray->data = NULL;
    tok_darray->len = 0;
    tok_darray->cap = 0;
    return tok_darray_set_cap(tok_darray, cap);
}
static int tok_darray_deinit(struct tok_darray *tok_darray) {
    free(tok_darray->data);
    tok_darray->data = NULL;
    tok_darray->len = 0;
    tok_darray->cap = 0;
    return TOK_DARRAY_OK;
}
static int tok_darray_push(struct tok_darray *tok_darray, tok_darray_elem_type *elem) {
    int rv;
    if (tok_darray->len + 1 > tok_darray->cap) {
        if (!tok_darray->cap)
            tok_darray->cap = 1;
        rv = tok_darray_set_cap(tok_darray, tok_darray->cap * 2);
        if (rv != TOK_DARRAY_OK) {
            return rv;
        }
    }
    memcpy(tok_darray->data + tok_darray->len, elem, sizeof *elem);
    tok_darray->len++;
    return TOK_DARRAY_OK;
}
#endif //TOK_DARRAY_H
