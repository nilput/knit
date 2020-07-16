#ifndef KNIT_H
#define KNIT_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> //need uintptr_t
#include <stdarg.h>
#include <stdio.h>
//there are other includes in the file. 

#include "kdata.h" //data structures
#include "knit_util.h" 
#include "knit_gc.h" 
#include "knit_bitset.h" 
#include "knit_mem_stats.h"

/*
  ARC macros, currently not used in a meaningful way,
  in the future ARC might be used as an alternative to garbage collection
*/
#define kincref(p) knitx_obj_incref(knit, (struct knit_obj *)(p))
#define kdecref(p) knitx_obj_decref(knit, (struct knit_obj *)(p))
#define ktobj(p) ((struct knit_obj *) (p))
#define kunreachable()

//forward declarations
struct knit_lex;
struct knit_prs;

static const char *knitx_obj_type_name(struct knit *knit, struct knit_obj *obj);
static inline int knitx_op_do_test_binop(struct knit *knit, struct knit_obj *a, struct knit_obj *b, int op);
static int kexpr_expr(struct knit *knit, struct knit_prs *prs, int min_prec);
static int knit_error(struct knit *knit, int err_type, const char *fmt, ...);
static int knitx_emit_expr_eval(struct knit *knit, struct knit_prs *prs, struct knit_expr *expr, int eval_ctx, int nexpected); //fwd
static int knitx_emit_ret(struct knit *knit, struct knit_prs *prs, int count);
static int knitx_expr(struct knit *knit, struct knit_prs *prs);
static int knitx_expr_destroy(struct knit *knit, struct knit_prs *prs, struct knit_expr *prs_expr);
static int knitx_int_new(struct knit *knit, struct knit_int **integerp_out, int value);
static int knitx_int_new_gcobj(struct knit *knit, struct knit_int **integerp_out, int value);
static int knitx_lexer_deinit(struct knit *knit, struct knit_lex *lxr);
static int knitx_lexer_init_str(struct knit *knit, struct knit_lex *lxr, const char *program);
static int knitx_lexer_peek_cur(struct knit *knit, struct knit_lex *lxr, struct knit_tok **tokp);
static int knitx_lexer_peek_la(struct knit *knit, struct knit_lex *lxr, struct knit_tok **tokp);
static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj);
static int knitx_obj_rep(struct knit *knit, struct knit_obj *obj, struct knit_str *outi_str, int human);
static int knitx_prs_if_stmt_new(struct knit *knit, struct knit_prs *prs, struct knit_stmt **if_stmt_out);
static int knitx_stmt_array_emit(struct knit *knit, struct knit_prs *prs, struct knit_stmt_darray *array);
static int knitx_stmt_prs_emit(struct knit *knit, struct knit_prs *prs, int allowed_stmts);
static int knitx_stmt_prs_new(struct knit *knit, struct knit_prs *prs, int allowed_stmts, struct knit_stmt **stmt_out); //fwd
static int knitx_str_new_copy(struct knit *knit, struct knit_str **strp, struct knit_str *src);
static int knitx_str_new_copy_gcobj(struct knit *knit, struct knit_str **strp, struct knit_str *src);
static int knitx_str_strcpy(struct knit *knit, struct knit_str *str, const char *src0);
static int knitx_tok_extract_to_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *str);
static int knitx_str_strlcpy(struct knit *knit, struct knit_str *str, const char *src, int srclen);

/*
    hashtable functions
*/
    static int knit_vars_hasht_key_eq_cmp(knit_vars_hasht_key_type *key_1, knit_vars_hasht_key_type *key_2) {
        struct knit_str *str1 = key_1;
        struct knit_str *str2 = key_2;
        if (str1->len != str2->len)
            return 1;
        return memcmp(str1->str, str2->str, str1->len); //0 if eq
    }
    static size_t knit_vars_hasht_hash(knit_vars_hasht_key_type *key) {
        struct knit_str *str = key;
        return SuperFastHash(str->str, str->len);
    }

    //boolean, 1 means eq, 0 uneq
    static int knitx_obj_eq(struct knit *knit, struct knit_obj *obj_a, struct knit_obj *obj_b) {
        int rv = knitx_op_do_test_binop(knit, obj_a, obj_b, KTESTEQ);
        if (rv == KNIT_OK && knit->ex.last_cond) {
            return 1;
        }
        return 0;
    }
    static size_t knitx_obj_hash(struct knit *knit, struct knit_obj *obj) {
        switch (obj->u.ktype) {
            case KNIT_INT:
                return obj->u.integer.value;
            case KNIT_STR:
                /*defined in hasht third_party/ */
                return SuperFastHash(obj->u.str.str, obj->u.str.len);
            default:
                return knit_error(knit, KNIT_RUNTIME_ERR, "hashing %s types is not implemented", knitx_obj_type_name(knit, obj));
        }
        kunreachable();
        return 0;
    }
    
    static int kobj_hasht_key_eq_cmp(void *udata, kobj_hasht_key_type *key_1, kobj_hasht_key_type *key_2) {
        struct knit *knit = (struct knit *) udata;
        knit_assert_h(!!knit, "NULL was passed to kobj_hasht_key_eq_cmp");
        //we are comparing two knit_obj * pointers
        if (*key_1 == *key_2)
            return 0;  //eq
        return knitx_obj_eq(knit, *key_1, *key_2) ? 0 : 1; //0 if eq
    }
    static size_t kobj_hasht_hash(void *udata, kobj_hasht_key_type *key) {
        struct knit *knit = (struct knit *) udata;
        return knitx_obj_hash(knit, *key);
    }
/*end of hashtable functions*/


//enable checks that are only there for testing
#define KNIT_CHECKS

//#KNIT_DEBUG_PRINT for debug output (lexer tokens, code, etc..)
static int KNIT_DBG_PRINT = 0;

//these are the only instances of these special types
static const struct knit_bvalue ktrue  = { KNIT_TRUE };
static const struct knit_bvalue kfalse = { KNIT_FALSE };
static const struct knit_bvalue knull  = { KNIT_NULL };

static void knit_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void knit_assert_s(int condition, const char *fmt, ...) {
    va_list ap;
    if (!condition) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        va_end(ap);
        assert(0);
    }
}

static void knit_assert_h(int condition, const char *fmt, ...) {
    va_list ap;
    if (!condition) {
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        va_end(ap);
        assert(0);
    }
}

static struct knit_str *knit_as_str(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_STR, "knit_as_str(): invalid argument type, expected string");
    return &obj->u.str;
}

static struct knit_list *knit_as_list(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_LIST, "knit_as_list(): invalid argument type, expected list");
    return &obj->u.list;
}

static void knitx_obj_incref(struct knit *knit, struct knit_obj *obj) {
}

static void knitx_obj_decref(struct knit *knit, struct knit_obj *obj) {
}


//TODO make sure dest,src order is consistent in functions
//does a shallow copy
static int knitx_obj_copy(struct knit *knit, struct knit_obj **dest, struct knit_obj *src) {
    int rv = KNIT_OK;
    struct knit_int *new_int;
    struct knit_str *new_str;
    switch (src->u.ktype) {
        case KNIT_INT:
            rv = knitx_int_new_gcobj(knit, &new_int, src->u.integer.value); 
            if (rv != KNIT_OK) 
                return rv;
            *dest = (struct knit_obj *)new_int;
            return KNIT_OK;
        case KNIT_STR:
            rv = knitx_str_new_copy_gcobj(knit, &new_str, (struct knit_str *)src); 
            if (rv != KNIT_OK) 
                return rv;
            *dest = (struct knit_obj *)new_str;
            return KNIT_OK;
        default:
            *dest = NULL;
            return knit_error(knit, KNIT_RUNTIME_ERR, "copying %s types is not implemented", knitx_obj_type_name(knit, src));
    }
    kunreachable();
    return KNIT_RUNTIME_ERR;
}

// Assumes the length of both was checked and it was equal!
static int knit_strl_eq(const char *a, const char *b, size_t len) {
    return memcmp(a, b, len) == 0;
}

static int knitx_rstrdup(struct knit *knit, const char *s, char **out_s) {
    (void) knit;
    knit_assert_h(!!s, "passed null string to knitx_rstrdup()");
    size_t len = strlen(s);
    void *p;
    int rv = knitx_rmalloc(knit, len + 1, &p);
    *out_s = p;
    if (rv != KNIT_OK) {
        return rv;
    }
    memcpy(*out_s, s, len);
    (*out_s)[len] = 0;
    return KNIT_OK;
}

static int knitx_list_init(struct knit *knit, struct knit_list *list, int isz) {
    int rv = KNIT_OK;
    if (isz < 0)
        isz = 0;
    void *p = NULL;
    if (isz > 0) {
        rv = knitx_tmalloc(knit, sizeof(struct knit_obj *) * isz, &p); 
        if (rv != KNIT_OK)
            return rv;
    }
    list->ktype = KNIT_LIST;
    list->items = p;
    list->len = 0;
    list->cap = isz;
    return KNIT_OK;
}

static int knitx_list_deinit(struct knit *knit, struct knit_list *list) {
    int rv = KNIT_OK;
    if (list->items) {
        rv = knitx_tfree(knit, list->items);
    }
    return rv;
}

static int knitx_list_new_gcobj(struct knit *knit, struct knit_list **list, int isz) {
    int rv = KNIT_OK;
    void *p;
    p = knit_gc_new_object(knit);
    if (!p) {
        *list = p;
        return KNIT_GC_NOMEM;
    }
    rv = knitx_list_init(knit, p, isz); 
    if (rv != KNIT_OK) {
        knit_gc_obj_null(knit, p);
        return rv;
    }
    *list = p;
    return KNIT_OK;
}

static int knitx_list_destroy(struct knit *knit, struct knit_list *list) {
    //TODO: for obj in items destroy/deref obj
    knitx_list_deinit(knit, list);
    knitx_tfree(knit, list);
    return KNIT_OK;
}

static int knitx_list_resize(struct knit *knit, struct knit_list *list, int new_sz) {
    knit_assert_h(list->cap >= list->len, "");
    void *p = NULL;
    int rv = knitx_trealloc(knit, list->items, sizeof(struct knit_obj *) * new_sz, &p);
    if (rv != KNIT_OK) {
        return rv;
    }
    list->items = p;
    list->cap = new_sz;
    return KNIT_OK;
}

static int knitx_list_push(struct knit *knit, struct knit_list *list, struct knit_obj *obj) {
    int rv = KNIT_OK;
    if (list->len >= list->cap) {
        rv = knitx_list_resize(knit, list, list->cap == 0 ? 8 : list->cap * 2); 
        if (rv != KNIT_OK)
            return rv;
    }
    list->items[list->len++] = obj;
    return KNIT_OK;
}

static int knitx_list_pop(struct knit *knit, struct knit_list *list) {
    if (list->len <= 0) {
        return knit_error(knit, KNIT_OUT_OF_RANGE_ERR, "knitx_list_pop(): trying to pop from an empty list");
    }
    list->len--;
    return KNIT_OK;
}

static int knitx_dict_init(struct knit *knit, struct knit_dict *dict, int isz) {
    int rv = KNIT_OK;
    if (isz < 0)
        isz = 0;
    rv = kobj_hasht_init_with_udata(&dict->ht, isz, knit);

    if (rv != KOBJ_HASHT_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize dict hashtable");
    }
    dict->ktype = KNIT_DICT;
    dict->ht.userdata = knit;
    return KNIT_OK;
}

static int knitx_dict_deinit(struct knit *knit, struct knit_dict *dict) {
    kobj_hasht_deinit(&dict->ht);
    return KNIT_OK;
}

static int knitx_dict_new_gcobj(struct knit *knit, struct knit_dict **dict, int isz) {
    int rv = KNIT_OK;
    void *p = knit_gc_new_object(knit);
    if (!p) {
        *dict = p;
        return KNIT_GC_NOMEM;
    }
    rv = knitx_dict_init(knit, p, isz); 
    if (rv != KNIT_OK) {
        knit_gc_obj_null(knit, p);
        return rv;
    }
    *dict = p;
    return KNIT_OK;
}

static int knitx_dict_destroy(struct knit *knit, struct knit_dict *dict) {
    //TODO: for obj in items destroy/deref obj
    knitx_dict_deinit(knit, dict);
    knitx_tfree(knit, dict);
    return KNIT_OK;
}

static int knitx_dict_lookup(struct knit *knit, struct knit_dict *dict, struct knit_obj *key, struct knit_obj **value_out) {
    struct kobj_hasht_iter iter;
    int rv = kobj_hasht_find(&dict->ht, &key, &iter);
#ifdef KNIT_CHECKS
    *value_out = NULL;
#endif
    if (rv != KOBJ_HASHT_OK) {
        if (rv == KOBJ_HASHT_NOT_FOUND)
            return KNIT_NOT_FOUND;
        else 
            return knit_error(knit, KNIT_RUNTIME_ERR, "an error occured while trying to lookup a dict key in kobj_hasht_find()");
    }
    *value_out = iter.pair->value;
    return KNIT_OK;
}

static int knitx_dict_set(struct knit *knit, struct knit_dict *dict, struct knit_obj *key, struct knit_obj *value) {
    struct kobj_hasht_iter iter;
    int rv = kobj_hasht_find(&dict->ht, &key, &iter);
    if (rv == KOBJ_HASHT_OK) {
        //todo destroy previous value 
        iter.pair->value = value;
    }
    else if (rv == KOBJ_HASHT_NOT_FOUND) {
        struct knit_obj *new_key = NULL;
        rv = knitx_obj_copy(knit, &new_key, key); 
        if (rv != KNIT_OK)
            return rv;
        rv = kobj_hasht_insert(&dict->ht, &new_key, &value);
    }
    else {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_dict_set(): assignment failed");
    }
    return KNIT_OK;
}

static int knitx_int_init(struct knit *knit, struct knit_int *integer, int value) {
    integer->ktype = KNIT_INT;
    integer->value = value;
    return KNIT_OK;
}

static int knitx_int_deinit(struct knit *knit, struct knit_int *integer) {
    return KNIT_OK;
}

static int knitx_int_new_gcobj(struct knit *knit, struct knit_int **integerp_out, int value) {
    void *p = knit_gc_new_object(knit);
    if (!p) {
        return KNIT_GC_NOMEM;
    }
    *integerp_out = p;
    return knitx_int_init(knit, *integerp_out, value);
}

static int knitx_int_destroy(struct knit *knit, struct knit_int *integer, int value) {
    int rv = knitx_int_deinit(knit, integer);
    int rv2 = knitx_tfree(knit, integer);
    if (rv != KNIT_OK)
        return rv;
    return rv2;
}

//see valid string states at kdata.h
static int knitx_str_init(struct knit *knit, struct knit_str *str) {
    (void) knit;
    str->ktype = KNIT_STR;
    str->str = "";
    str->cap = -1;
    str->len = 1;
    return KNIT_OK;
}

//src0 must outlive the object (doesn't own it)
static int knitx_str_init_const_str(struct knit *knit, struct knit_str *str, const char *src0) {
    (void) knit;
    knit_assert_h(!!src0, "passed NULL string");
    str->ktype = KNIT_INT;
    str->str = (char *) src0;
    str->len = strlen(src0);
    str->cap = -1;
    return KNIT_OK;
}

static int knitx_str_deinit(struct knit *knit, struct knit_str *str) {
    if (str->cap >= 0) {
        knitx_tfree(knit, str->str);
    }
    str->str = NULL;
    str->cap = 0;
    str->len = 0;
    return KNIT_OK;
}

static int knitx_str_new(struct knit *knit, struct knit_str **strp) {
    void *p = NULL;
    int rv = knitx_tmalloc(knit, sizeof(struct knit_str), &p);
    if (rv != KNIT_OK) {
        *strp = NULL;
        return rv;
    }
    rv = knitx_str_init(knit, p);
    *strp = p;
    return rv;
}

static int knitx_str_new_gcobj(struct knit *knit, struct knit_str **strp) {
    void *p = knit_gc_new_object(knit);
    if (!p) {
        *strp = NULL;
        return KNIT_GC_NOMEM;
    }
    int rv = knitx_str_init(knit, p);
    if (rv != KNIT_OK) {
        knit_gc_obj_null(knit, p);
        return rv;
    }
    *strp = p;
    return rv;
}

static int knitx_str_destroy(struct knit *knit, struct knit_str *strp) {
    int rv = knitx_str_deinit(knit, strp);
    int rv2 = knitx_tfree(knit, strp);
    if (rv != KNIT_OK)
        return rv;
    return rv2;
}

static int knitx_str_new_strcpy(struct knit *knit, struct knit_str **strp, const char *src0) {
    int rv = knitx_str_new(knit, strp);
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_strcpy(knit, *strp, src0);
    if (rv != KNIT_OK) {
        *strp = NULL;
        return rv;
    }
    return KNIT_OK;
}

static int knitx_str_new_strlcpy_gcobj(struct knit *knit, struct knit_str **strp, const char *src0, int len) {
    int rv = knitx_str_new_gcobj(knit, strp);
    if (rv == KNIT_OK)
        rv = knitx_str_strlcpy(knit, *strp, src0, len);
    if (rv != KNIT_OK) {
        *strp = NULL;
        return rv;
    }
    return KNIT_OK;
}

static int knitx_str_new_strcpy_gcobj(struct knit *knit, struct knit_str **strp, const char *src0) {
    int rv = knitx_str_new_gcobj(knit, strp);
    if (rv == KNIT_OK)
        rv = knitx_str_strcpy(knit, *strp, src0);
    if (rv != KNIT_OK) {
        *strp = NULL;
        return rv;
    }
    return KNIT_OK;
}

static int knitx_str_clear(struct knit *knit, struct knit_str *str) {
    (void) knit;
    if (str->cap > 0) {
        str->str[0] = 0;
        str->len = 0;
    }
    else {
        str->str = "";
        str->len = 0;
        str->cap = -1;
    }
    return KNIT_OK;
}

static int knitx_str_set_cap(struct knit *knit, struct knit_str *str, int capacity) {
    if (capacity == 0) {
        return knitx_str_clear(knit, str);
    }
    if (str->cap < 0) {
        void *p;
        int rv  = knitx_tmalloc(knit, capacity, &p);
        if (rv != KNIT_OK) {
            return rv;
        }
        memcpy(p, str->str, str->len);
        str->str = p;
        str->str[str->len] = 0;
    }
    else {
        void *p = NULL;
        int rv = knitx_trealloc(knit, str->str, capacity, &p);
        if (rv != KNIT_OK) {
            return rv;
        }
        str->str = p;
    }
    str->cap = capacity;
    return KNIT_OK;
}

static int knitx_str_strlcpy(struct knit *knit, struct knit_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= srclen) {
        rv = knitx_str_set_cap(knit, str, srclen + 1);
        if (rv != KNIT_OK) {
            return rv;
        }
    }
    memcpy(str->str, src, srclen);
    str->len = srclen;
    str->str[str->len] = 0;
    return KNIT_OK;
}

static int knitx_str_strlappend(struct knit *knit, struct knit_str *str, const char *src, int srclen) {
    int rv;
    if (str->cap <= (str->len + srclen)) {
        rv = knitx_str_set_cap(knit, str, str->len + srclen + 1);
        if (rv != KNIT_OK) {
            return rv;
        }
    }
    memcpy(str->str + str->len, src, srclen);
    str->len += srclen;
    str->str[str->len] = 0;
    return KNIT_OK;
}

static int knitx_str_strcpy(struct knit *knit, struct knit_str *str, const char *src0) {
    return knitx_str_strlcpy(knit, str, src0, strlen(src0));
}

static int knitx_str_strappend(struct knit *knit, struct knit_str *str, const char *src0) {
    return knitx_str_strlappend(knit, str, src0, strlen(src0));
}

static int knitx_str_init_copy(struct knit *knit, struct knit_str *str, struct knit_str *src) {
    int rv = knitx_str_init(knit, str); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_strlcpy(knit, str, src->str, src->len);
    if (rv != KNIT_OK) {
        knitx_str_deinit(knit, str);
        return rv;
    }
    return KNIT_OK;
}

static int knitx_str_new_copy(struct knit *knit, struct knit_str **strp, struct knit_str *src) {
    int rv = knitx_str_new(knit, strp);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_str_strlcpy(knit, *strp, src->str, src->len);
    if (rv != KNIT_OK) {
        knitx_str_destroy(knit, *strp);
        *strp = NULL;
    }
    return KNIT_OK;
}

static int knitx_str_new_copy_gcobj(struct knit *knit, struct knit_str **strp, struct knit_str *src) {
    int rv = knitx_str_new_gcobj(knit, strp);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_str_strlcpy(knit, *strp, src->str, src->len);
    if (rv != KNIT_OK) {
        knitx_str_destroy(knit, *strp);
        *strp = NULL;
    }
    return KNIT_OK;
}

//exclusive end, inclusive begin
static int knitx_str_mutsubstr(struct knit *knit, struct knit_str *str, int begin, int end) {
    knit_assert_h((begin <= end) && (begin <= str->len) && (end <= str->len), "invalid arguments to mutsubstr()");
    void *p = NULL;
    int len = end - begin;
    int rv  = knitx_tmalloc(knit, len + 1, &p); 
    if (rv != KNIT_OK)
        return rv;
    memcpy(p, str->str + begin, len);
    knitx_tfree(knit, str->str);
    str->str = p;
    str->len = len;
    str->str[len] = 0;
    return KNIT_OK;
}

enum knitx_str_mutstrip {
    KNITX_STRIP_DEFAULT = 0,
    KNITX_STRIP_LEFT = 1,
    KNITX_STRIP_RIGHT = 2,
};

//stripchars are treated like a set, each assumed to be one char to be excluded repeatedly from beginning and end
static int knitx_str_mutstrip(struct knit *knit, struct knit_str *str, const char *stripchars, enum knitx_str_mutstrip opts) {
    if (!opts)
        opts = KNITX_STRIP_LEFT + KNITX_STRIP_RIGHT;
    int begin = 0;
    int end = str->len;
    if (opts & KNITX_STRIP_LEFT) {
        for (int i=0; i < str->len && strchr(stripchars, str->str[i]); i++)
            begin++;
    }
    if (opts & KNITX_STRIP_RIGHT) {
        for (int i=str->len - 1; i > begin && strchr(stripchars, str->str[i]); i--)
            end--;
    }
    if (begin == 0 && end == str->len)
        return KNIT_OK;
    return knitx_str_mutsubstr(knit, str, begin, end);
}

//boolean
static int knitx_str_streqc(struct knit *knit, struct knit_str *str, const char *src0) {
    if (!str->len && !src0[0])
        return 1;
    if (!str->len || !src0[0])
        return 0;
    knit_assert_h(!!str->str, "invalid string passed to be compared");
    return strcmp(str->str, src0) == 0;
}

//boolean
static int knitx_str_streq(struct knit *knit, struct knit_str *str_a, struct knit_str *str_b) {
    if (str_a->len != str_b->len)
        return 0;
    else if (str_a->len == 0)
        return 1;
    return knit_strl_eq(str_a->str, str_b->str, str_a->len);
}

static int knitx_str_init_strcpy(struct knit *knit, struct knit_str *str, const char *src0) {
    int rv = knitx_str_init(knit, str);
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_strcpy(knit, str, src0);
    if (rv != KNIT_OK) {
        knitx_str_deinit(knit, str);
        return rv;
    }
    return KNIT_OK;
}

static int knitx_getvar_(struct knit *knit, const char *varname, struct knit_obj **objp) {

    struct knit_str key;
    int rv = knitx_str_init_const_str(knit, &key, varname);
    if (rv != KNIT_OK) {
        return rv;
    }
    struct knit_exec_state *exs = &knit->ex;
    struct knit_vars_hasht_iter iter;
    rv = knit_vars_hasht_find(&exs->global_ht, &key, &iter);
    if (rv != KNIT_VARS_HASHT_OK) {
        if (rv == KNIT_VARS_HASHT_NOT_FOUND)
            return knit_error(knit, KNIT_NOT_FOUND, "variable '%s' is undefined", varname);
        else 
            return knit_error(knit, KNIT_RUNTIME_ERR, "an error occured while trying to lookup a variable in knit_vars_hasht_find()");
    }
    *objp = iter.pair->value;
    return KNIT_OK;
}

static int knitx_vardump(struct knit *knit, const char *varname) {
    fprintf(stderr, "Vardump:\n");
    struct knit_obj *valpo;
    int rv = knitx_getvar_(knit, varname, &valpo);
    if (rv == KNIT_OK) {
        if (valpo->u.ktype == KNIT_STR) {
            struct knit_str *valp = &valpo->u.str;
            fprintf(stderr, "'%s'", valp->str);
        }
        else if (valpo->u.ktype == KNIT_LIST) {
            fprintf(stderr, "LIST");
        }
        else if (valpo->u.ktype == KNIT_INT) {
            struct knit_int *vali = &valpo->u.integer;
            fprintf(stderr, "%d", vali->value);
        }
        else {
            fprintf(stderr, "[%s object]", knitx_obj_type_name(knit, valpo));
        }
    }
    else {
        fprintf(stderr, "NULL");
    }
    fprintf(stderr, "\n");
    return rv;
}

static int knitx_globals_dump(struct knit *knit) {
    struct knit_vars_hasht *ht = &knit->ex.global_ht;
    struct knit_vars_hasht_iter iter;
    int rv = knit_vars_hasht_begin_iterator(ht, &iter);
    fprintf(stderr, "Global variables:\n");
    for (; knit_vars_hasht_iter_check(&iter); knit_vars_hasht_iter_next(ht, &iter)) {
        if (iter.pair->key.str) {
            fprintf(stderr, "\t%s", iter.pair->key.str);
        }
        else {
            fprintf(stderr, "\tNULL");
        }
        fprintf(stderr, " : ");
        if (iter.pair->value) {
            knitx_obj_dump(knit, iter.pair->value);
        }
        else {
            fprintf(stderr, "NULL");
        }
    }
    return KNIT_OK;
}

static int knit_vsprintf(struct knit *knit, struct knit_str *str, const char *fmt, va_list ap_in) {
    int fmtlen = strlen(fmt);
    int rv;
    va_list ap;
    if (str->cap < 2) {
        rv = knitx_str_set_cap(knit, str, 2);
        if (rv != KNIT_OK) {
            return rv;
        }
    }
    va_copy(ap, ap_in);
    int needed = vsnprintf(str->str, str->cap, fmt, ap);
    va_end(ap);
    if (needed >= str->cap) {
        rv = knitx_str_set_cap(knit, str, needed + 1);
        if (rv != KNIT_OK) {
            return rv;
        }
        knit_assert_s(needed < str->cap, "str grow failed");
        va_copy(ap, ap_in);
        needed = vsnprintf(str->str, str->cap, fmt, ap);
        knit_assert_s(needed < str->cap, "str grow failed");
        va_end(ap);
    }
    str->len = needed;
    return KNIT_OK;
}

static int knit_sprintf(struct knit *knit, struct knit_str *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rv = knit_vsprintf(knit, str, fmt, ap);
    va_end(ap);
    return rv;
}

static void knit_clear_error(struct knit *knit) {
    knit->err = 0;
    if (knit->is_err_msg_owned)
        knitx_rfree(knit, knit->err_msg);
    knit->err_msg = NULL;
    knit->is_err_msg_owned = 0;
}

static void knit_set_error_policy(struct knit *knit, int policy) {
    knit_assert_h(policy == KNIT_POLICY_CONTINUE || policy == KNIT_POLICY_EXIT, "invalid error policy");
    knit->err_policy = policy;
}

static void knit_error_act(struct knit *knit, int err_type) {
    (void) err_type;
    if (knit->err_policy == KNIT_POLICY_EXIT) {
        if (knit->err_msg)
            fprintf(stderr, "%s\n", knit->err_msg);
        else
            fprintf(stderr, "an unknown error occured (no err msg)\n");
        exit(1);
    }
    else {
        knit_fatal("other error handling policies are not implemented");
    }
}

static int knit_error(struct knit *knit, int err_type, const char *fmt, ...) {
    knit_assert_h(err_type, "invalid error code passed (0)");
    if (knit->err)
        return knit->err;
    knit->err = err_type;
    va_list ap;
    struct knit_str tmp;
    int rv = knitx_str_init(knit, &tmp);
    if (rv != KNIT_OK) {
        return rv;
    }
    va_start(ap, fmt);
    rv = knit_vsprintf(knit, &tmp, fmt, ap);
    va_end(ap);
    if (rv == KNIT_OK) {
        knit_assert_h(!!tmp.str, "");
        rv = knitx_rstrdup(knit, tmp.str, &knit->err_msg);
    }
    knitx_str_deinit(knit, &tmp);
    knit_error_act(knit, err_type);
    return err_type;
}

static int knitx_set_str(struct knit *knit, const char *key, const char *value) {
    struct knit_str key_str;
    int rv = knitx_str_init(knit, &key_str);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_str_strcpy(knit, &key_str, key);
    if (rv != KNIT_OK)
        goto cleanup_key;
    struct knit_str *val_strp;
    rv = knitx_str_new_gcobj(knit, &val_strp);
    if (rv != KNIT_OK) 
        goto cleanup_key;
    rv = knitx_str_strcpy(knit, val_strp, value);
    if (rv != KNIT_OK) 
        goto cleanup_val;
    //ownership of key is transferred to the vars hashtable
    struct knit_obj *objp = (struct knit_obj *) val_strp; //defined operation?
    struct knit_exec_state *exs = &knit->ex;
    rv = knit_vars_hasht_insert(&exs->global_ht, &key_str, &objp);
    if (rv != KNIT_VARS_HASHT_OK) {
        rv = knit_error(knit, KNIT_RUNTIME_ERR, "knitx_set_str(): inserting key into vars hashtable failed");
        goto cleanup_val;
    }
    return KNIT_OK;

cleanup_val:
    knitx_str_destroy(knit, val_strp);
cleanup_key:
    knitx_str_deinit(knit, &key_str);
    return rv;
}

static int knitx_get_str(struct knit *knit, const char *varname, struct knit_str **kstrp) {
    struct knit_obj *objp;
    *kstrp = NULL;
    int rv = knitx_getvar_(knit, varname, &objp);
    if (rv != KNIT_OK)
        return rv;
    *kstrp = knit_as_str(objp);
    return KNIT_OK;
}

static int knitx_type_str_get_property(struct knit *knit, struct knit_str *property_name, struct knit_obj **obj_out) {
    if (property_name->len == 5 && knit_strl_eq(property_name->str, "strip", 5)) {
        *obj_out = (struct knit_obj *) &kbuiltins.kstr.strip;
        return KNIT_OK;
    }
    *obj_out = NULL;
    return knit_error(knit, KNIT_UNDEFINED, "knitx_type_str_get_property(): property %s is not defined", property_name->str);
}

static int knitx_type_list_get_property(struct knit *knit, struct knit_str *property_name, struct knit_obj **obj_out) {
    if (property_name->len == 6 && knit_strl_eq(property_name->str, "append", 6)) {
        *obj_out = (struct knit_obj *) &kbuiltins.klist.append;
        return KNIT_OK;
    }
    *obj_out = NULL;
    return knit_error(knit, KNIT_UNDEFINED, "knitx_type_list_get_property(): property %s is not defined", property_name->str);
}

static int knitx_obj_get_property(struct knit *knit, struct knit_obj *obj, struct knit_str *name, struct knit_obj **obj_out) {
    if (obj->u.ktype == KNIT_STR) {
        return knitx_type_str_get_property(knit, name, obj_out);
    }
    else if (obj->u.ktype == KNIT_LIST) {
        return knitx_type_list_get_property(knit, name, obj_out);
    }
    else {
        return knit_error(knit, KNIT_RUNTIME_ERR, "cannot get a property out of this type of object");
    }
}

static int knitx_lexer_init(struct knit *knit, struct knit_lex *lxr) {
    lxr->lineno = 1;
    lxr->colno = 1;
    lxr->offset = 0;
    lxr->tokno = 0;
    lxr->pbcd = 0;
    int rv = tok_darray_init(&lxr->tokens, 100);
    return rv;
}


static int knitx_lexer_init_str(struct knit *knit, struct knit_lex *lxr, const char *program) {
    int rv = knitx_lexer_init(knit, lxr);
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_new_strcpy(knit, &lxr->input, program);
    if (rv != KNIT_OK)
        goto lexer_cleanup;
    rv = knitx_str_new_strcpy(knit, &lxr->filename, "<str-input>");
    if (rv != KNIT_OK)
        goto input_cleanup;
    return KNIT_OK;

input_cleanup:
    knitx_str_destroy(knit, lxr->input);
lexer_cleanup:
    knitx_lexer_deinit(knit, lxr);
    return rv;
}

static int knitx_lexer_deinit(struct knit *knit, struct knit_lex *lxr) {
    knitx_str_destroy(knit, lxr->input);
    lxr->input = NULL;
    tok_darray_deinit(&lxr->tokens);
    return KNIT_OK;
}

/*
 * parser gets 
 * case
 *      foo = 'bar'
 * todo:
 *      primaryexp
 *      no suffix
 *      assignment -> lhs: exp
 *      rhs -> exp
 *
 * to exec:
 *      get location of foo
 *      execute exp
 *      set the value of foo to exp
 *
 * case
 *      a = 2 + 4 * 3
 *      primary exp
 *      no suffix
 *      assignemnt -> lhs : exp
 *      rhs -> exp
 *              add
 *              2   mul
 *                  4  3
 *
*/

static int knitx_block_init(struct knit *knit, struct knit_block *block) {
    int rv = insns_darray_init(&block->insns, 256);
    if (rv != INSNS_DARRAY_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_init(): initializing statements darray failed");
    }
    rv = knit_objp_darray_init(&block->constants, 128);
    if (rv != KNIT_OK)
        goto fail_objp_darray;
    block->nargs = 0;
    block->nlocals = 0;
    return KNIT_OK;

fail_objp_darray:
    insns_darray_deinit(&block->insns);
    return rv;
}

static int knitx_block_add_insn(struct knit *knit, struct knit_block *block, struct knit_insn *insn) {
    int rv = insns_darray_push(&block->insns, insn);
    if (rv != INSNS_DARRAY_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_add_insn(): adding a insn to insns darray failed");
    }
    return KNIT_OK;
}

//block owns allocd_obj, it is expected to be a tmallocd ptr (TODO check)
static int knitx_block_add_constant(struct knit *knit, struct knit_block *block, struct knit_obj *allocd_obj, int *index_out) {
    int rv = knit_objp_darray_push(&block->constants, &allocd_obj);
    if (rv != KNIT_OBJP_DARRAY_OK) {
        *index_out = -1;
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_add_constant(): adding a constant to darray failed");
    }
    knit_assert_s(block->constants.len > 0, "darray push silently failed");
    *index_out = block->constants.len - 1;
    return KNIT_OK;
}

//boolean return value
static int knitx_is_in_filescope(struct knit *knit, struct knit_prs *prs) {
    return prs->curblk->parent == NULL;
}

static int knit_parse_error(struct knit_prs *prs, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "line %d:", prs->lex.lineno);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    return KNIT_SYNTAX_ERR;
}

static int knit_runtime_error(struct knit *knit, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return KNIT_RUNTIME_ERR;
}

//doesn't own src
static int knitx_current_block_add_strl_constant(struct knit *knit, struct knit_prs *prs,  const char *src, int len, int *index_out) {
    struct knit_str *str = NULL;
    int rv = knitx_str_new(knit, &str); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_strlcpy(knit, str, src, len);  
    if (rv != KNIT_OK)
        return rv;
    return knitx_block_add_constant(knit, &prs->curblk->block, ktobj(str), index_out);
}

static int knitx_block_deinit(struct knit *knit, struct knit_block *block) {
    insns_darray_deinit(&block->insns);
    return KNIT_OK;
}

//never returns null
static const char *knitx_obj_type_name(struct knit *knit, struct knit_obj *obj) {
    if (obj->u.ktype == KNIT_INT)      return "KNIT_INT";
    else if (obj->u.ktype == KNIT_STR) return "KNIT_STR";
    else if (obj->u.ktype == KNIT_LIST) return "KNIT_LIST";
    return "ERR_UNKNOWN_TYPE";
}

//outi_str must be already initialized
static int knitx_obj_rep(struct knit *knit, struct knit_obj *obj, struct knit_str *outi_str, int human) {
    int rv = KNIT_OK;
    rv = knitx_str_clear(knit, outi_str); 
    if (rv != KNIT_OK)
        return rv;
    if (obj == NULL) {
        rv = knitx_str_strcpy(knit, outi_str, "[NULL]"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_STR) {
        struct knit_str *objstr = (struct knit_str *) obj;
        if (!human) {
            rv = knitx_str_strlcpy(knit, outi_str, "\"", 1); 
            if (rv != KNIT_OK)
                return rv;
        }
        rv = knitx_str_strlappend(knit, outi_str, objstr->str, objstr->len); 
        if (rv != KNIT_OK)
            return rv;
        if (!human) {
            rv = knitx_str_strlappend(knit, outi_str, "\"", 1); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (obj->u.ktype == KNIT_INT) {
        struct knit_int *objint = (struct knit_int *) obj;
        knit_sprintf(knit, outi_str, "%d", objint->value);
    }
    else if (obj->u.ktype == KNIT_LIST) {
        struct knit_list *objlist = (struct knit_list *) obj;
        rv = knitx_str_strlcpy(knit, outi_str, "[", 1); 
        if (rv != KNIT_OK)
            return rv;
        for (int i=0; i<objlist->len; i++) {
            if (i) {
                rv = knitx_str_strlappend(knit, outi_str, ", ", 2); 
                if (rv != KNIT_OK)
                    return rv;
            }
            struct knit_str *tmpstr = NULL;
            rv = knitx_str_new(knit, &tmpstr); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_obj_rep(knit, objlist->items[i], tmpstr, 0);
            if (rv != KNIT_OK) {
                knitx_str_destroy(knit, tmpstr);
                return rv;
            }
            rv = knitx_str_strlappend(knit, outi_str, tmpstr->str, tmpstr->len); 
            knitx_str_destroy(knit, tmpstr);
            if (rv != KNIT_OK)
                return rv;
        }
        rv = knitx_str_strlappend(knit, outi_str, "]", 1); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_DICT) {
        struct knit_dict *objdict = (struct knit_dict *) obj;
        rv = knitx_str_strlcpy(knit, outi_str, "{", 1); 
        if (rv != KNIT_OK)
            return rv;
        struct kobj_hasht_iter iter;
        kobj_hasht_begin_iterator(&objdict->ht, &iter);
        for (int i=0; kobj_hasht_iter_check(&iter); i++, kobj_hasht_iter_next(&objdict->ht, &iter)) 
        {
            struct knit_obj *key = iter.pair->key;
            struct knit_obj *value = iter.pair->value;
            if (i) {
                rv = knitx_str_strlappend(knit, outi_str, ", ", 2); 
                if (rv != KNIT_OK)
                    return rv;
            }
            struct knit_str *tmpstr = NULL;
            rv = knitx_str_new(knit, &tmpstr); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_obj_rep(knit, key, tmpstr, 0);
            if (rv != KNIT_OK) {
                knitx_str_destroy(knit, tmpstr);
                return rv;
            }
            rv = knitx_str_strlappend(knit, outi_str, tmpstr->str, tmpstr->len); 

            rv = knitx_str_strlappend(knit, outi_str, " : ", 3); 

            rv = knitx_obj_rep(knit, value, tmpstr, 0);
            if (rv != KNIT_OK) {
                knitx_str_destroy(knit, tmpstr);
                return rv;
            }
            rv = knitx_str_strlappend(knit, outi_str, tmpstr->str, tmpstr->len); 

            knitx_str_destroy(knit, tmpstr);
            if (rv != KNIT_OK)
                return rv;
        }

        rv = knitx_str_strlappend(knit, outi_str, "}", 1); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_CFUNC) {
        rv = knitx_str_strcpy(knit, outi_str, "<C function>"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_KFUNC) {
        rv = knitx_str_strcpy(knit, outi_str, "<knit function>"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_NULL) {
        rv = knitx_str_strcpy(knit, outi_str, "null"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_TRUE) {
        rv = knitx_str_strcpy(knit, outi_str, "true"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (obj->u.ktype == KNIT_FALSE) {
        rv = knitx_str_strcpy(knit, outi_str, "false"); 
        if (rv != KNIT_OK)
            return rv;
    }
    else {
        rv = knitx_str_strcpy(knit, outi_str, "<unknown type>"); 
        if (rv != KNIT_OK)
            return rv;
        return KNIT_WARNING;
    }
    return KNIT_OK;
}

static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj) {
    struct knit_str *tmpstr = NULL;
    int rv = knitx_str_new(knit, &tmpstr); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_obj_rep(knit, obj, tmpstr, 0);
    if (rv != KNIT_OK) {
        knitx_str_destroy(knit, tmpstr);
        return rv;
    }
    fprintf(stderr, "%s\n", tmpstr->str);
    knitx_str_destroy(knit, tmpstr);
    return KNIT_OK;
}

static int knitx_block_dump_consts(struct knit *knit, struct knit_block *block) { 
    fprintf(stderr, "Constants:\n");
    struct knit_str str;
    int rv = knitx_str_init(knit, &str); 
    if (rv != KNIT_OK)
        return rv;
    for (int i=0; i<block->constants.len; i++) {
        rv = knitx_obj_rep(knit, block->constants.data[i], &str, 0);
        if (rv != KNIT_OK) {
            knitx_str_deinit(knit, &str);
            return rv;
        }
        fprintf(stderr, "\tc[%d]: %s\n", i, str.str);
    }
    knitx_str_deinit(knit, &str);
    return KNIT_OK;
}

//outi_str must be already initialized
static int knitx_block_rep(struct knit *knit, struct knit_block *block, struct knit_str *outi_str) { 
    return knit_sprintf(knit, outi_str, "[block %p (c: %d, i: %d, L: %d)]", (void *) block, block->constants.len, block->insns.len, block->nlocals);
}

static int knitx_block_dump(struct knit *knit, struct knit_block *block) { 
    knitx_block_dump_consts(knit, block);
    fprintf(stderr, "Instructions:\n");
    for (int i=0; i<block->insns.len; i++) {
        struct knit_insn *insn = &block->insns.data[i];
        struct knit_insninfo *inf = &knit_insninfo[(int)insn->insn_type];
        fprintf(stderr, "\t%d\t%s", i, inf->rep);
        switch (inf->n_op) {
            case 1: {
                if (insn->insn_type == KEMIT) {
                    fprintf(stderr, " %s",  (insn->op1 == KEMTRUE)  ? "true"  :(
                                            (insn->op1 == KEMFALSE) ? "false" :(
                                            (insn->op1 == KEMNULL)  ? "null"  : "UNKNOWN")));
                }
                else {
                    fprintf(stderr, " %d",  insn->op1);
                }
            }/*fall through*/
            case 0: fprintf(stderr, "\n"); break; 
            default: knit_fatal("knitx_block_dump(): invalid no. operands"); break;
        }
    }
    return KNIT_OK;
}

static int knitx_varname_dump(struct knit *knit, struct knit_varname *vn) { 
    const char *loc = "UNK";
    switch (vn->location) {
        case KLOC_LOCAL_VAR: loc = "KLOC_LOCAL_VAR"; break;
        case KLOC_ARG:       loc = "KLOC_ARG";       break;
        case KLOC_GLOBAL_R:  loc = "KLOC_GLOBAL_R";  break;
        case KLOC_GLOBAL_RW: loc = "KLOC_GLOBAL_RW"; break;
        default: knit_assert_s(0,  "unreachable");   break;
    }
    fprintf(stderr, "\tname: %s\n"
                    "\t location: %s\n", vn->name.str, loc);
    if (vn->location == KLOC_ARG || vn->location == KLOC_LOCAL_VAR) {
        fprintf(stderr, "\t idx: %d\n", vn->idx);
    }
    return KNIT_OK;
}

//dump only block var info
static int knitx_curblock_dump(struct knit *knit, struct knit_curblk *curblk) { 
    fprintf(stderr, "Current Block Variables:\n");
    for (int i=0; i<curblk->locals.len; i++) {
        struct knit_varname *vn = curblk->locals.data + i;
        knitx_varname_dump(knit, vn);
    }
    return KNIT_OK;
}

/*EXECUTION STATE FUNCS*/
//initialize a frame for a call to a knit function
static int knitx_frame_init_kf(struct knit *knit, struct knit_frame *frame, struct knit_block *block, int ip, int bsp, int nargs, int nexpret) {
    kincref(block);
    frame->frame_type = KNIT_FRAME_KBLOCK;
    frame->u.kf.block = block;
    frame->u.kf.ip = ip;
    frame->bsp = bsp;
    frame->nargs = nargs;
    frame->nexpected_returns = nexpret;
    return KNIT_OK;
}

//initialize a frame for a call to a c function
static int knitx_frame_init_cf(struct knit *knit, struct knit_frame *frame, struct knit_cfunc *cfunc, int bsp, int nargs, int nexpret) {
    kincref(cfunc);
    frame->frame_type = KNIT_FRAME_CFUNC;
    frame->u.cf.cfunc = cfunc;
    frame->bsp = bsp;
    frame->nargs = nargs;
    frame->nexpected_returns = nexpret;
    return KNIT_OK;
}

static int knitx_frame_deinit(struct knit *knit, struct knit_frame *frame) {
    if (frame->frame_type == KNIT_FRAME_KBLOCK)
        kdecref(frame->u.kf.block);
    else if (frame->frame_type == KNIT_FRAME_CFUNC)
        kdecref(frame->u.cf.cfunc);
    else
        knit_assert_h(0, "invalid frame type");
    return KNIT_OK;
}

//outi_str must be already initialized
static int knitx_frame_rep(struct knit *knit, struct knit_frame *frame, struct knit_str *outi_str) {
    int rv = KNIT_OK;
    struct knit_str *tmpstr = NULL;
    rv = knitx_str_new(knit, &tmpstr); 
    if (rv != KNIT_OK)
        return rv;
    if (frame->frame_type == KNIT_FRAME_KBLOCK) {
        rv = knitx_block_rep(knit, frame->u.kf.block, tmpstr);
        if (rv != KNIT_OK) {
            goto cleanup_tmpstr;
        }
        rv = knit_sprintf(knit, outi_str, "[frame, block: %s, ip: %d, bsp: %d]", tmpstr->str, frame->u.kf.ip, frame->bsp);
    }
    else if (frame->frame_type == KNIT_FRAME_CFUNC) {
        rv = knit_sprintf(knit, outi_str, "[frame, c function ]");
    }
    else {
        knit_assert_h(0, "invalid frame type");
    }
    if (rv != KNIT_OK) {
        goto cleanup_tmpstr;
    }
    knitx_str_destroy(knit, tmpstr);
    return KNIT_OK;
cleanup_tmpstr:
    knitx_str_destroy(knit, tmpstr);
    return rv;
}

static int knitx_stack_init(struct knit *knit, struct knit_stack *stack) {
    int rv = knit_frame_darray_init(&stack->frames, 128);
    if (rv != KNIT_FRAME_DARRAY_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_init(): initializing frames stack failed");
    }
    rv = knit_objp_darray_init(&stack->vals, 512);
    if (rv != KNIT_OBJP_DARRAY_OK) {
        knit_frame_darray_deinit(&stack->frames);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_init(): initializing values stack failed");
    }
    return KNIT_OK;
}

static int knitx_stack_deinit(struct knit *knit, struct knit_stack *stack) {
    int rv1 = knit_objp_darray_deinit(&stack->vals);
    int rv  = knit_frame_darray_deinit(&stack->frames);
    if (rv1 != KNIT_OBJP_DARRAY_OK || rv != KNIT_FRAME_DARRAY_OK)
        return KNIT_DEINIT_ERR;
    return KNIT_OK;
}

//returns number of locals
static int knitx_stack_nlocals(struct knit *knit, struct knit_stack *stack) {
    struct knit_frame *top_frm = &stack->frames.data[stack->frames.len-1];
    if (top_frm->frame_type == KNIT_FRAME_KBLOCK) {
        return top_frm->u.kf.block->nlocals;
    }
    return 0;
}

//number of arguments passed to the current function being executed
//return value: integer
static int knitx_stack_nargs(struct knit *knit, struct knit_stack *stack) {
    struct knit_frame *top_frm = &stack->frames.data[stack->frames.len-1];
    return top_frm->nargs;
}

static int knitx_nargs(struct knit *knit) {
    return knitx_stack_nargs(knit, &knit->ex.stack);
}

//C-API
//sets *objp_out to a pointer to current function arguments[idx]
//return value: error code
static int knitx_stack_get_arg(struct knit *knit, struct knit_stack *stack, int idx, struct knit_obj **objp_out) {
    struct knit_frame *top_frm = &stack->frames.data[stack->frames.len-1];
    int nargs = knitx_stack_nargs(knit, stack);
    if (idx < 0 || idx >= nargs) {
        *objp_out = NULL;
        return knit_error(knit, KNIT_OUT_OF_RANGE_ERR, "knitx_stack_get_arg(): index out of range");
    }
    *objp_out = stack->vals.data[top_frm->bsp - idx - 2]; //-2 because we assume the function itself is pushed, see stack layout
    return KNIT_OK;
}

static int knitx_get_arg(struct knit *knit, int idx, struct knit_obj **objp_out) {
    return knitx_stack_get_arg(knit, &knit->ex.stack, idx, objp_out);
}

//number of temporaries for the last stack frame (excluding local function variables and function arguments)
static int knitx_stack_ntemp(struct knit *knit, struct knit_stack *stack) {
    struct knit_frame *top_frm = &stack->frames.data[stack->frames.len-1];
    return stack->vals.len - top_frm->bsp - knitx_stack_nlocals(knit, stack);
}

//C-API
//used by C functions to declare the number of values they're going to return
static void knitx_creturns(struct knit *knit, int nvalues) {
    struct knit_frame *top_frm = &knit->ex.stack.frames.data[knit->ex.stack.frames.len-1];
    knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= nvalues, "invalid number of returns");
    knit->ex.nresults = nvalues;
}

//if n_* is < 0 then all will be printed
//if it is zero nothing will be
static int knitx_stack_dump(struct knit *knit, struct knit_stack *stack, int n_values, int n_backtrace) {
    if (n_values < 0)
        n_values = stack->vals.len;
    if (n_backtrace < 0)
        n_backtrace = stack->frames.len;
    if (n_values > stack->vals.len || n_backtrace > stack->frames.len) {
        return knit_error(knit, KNIT_OUT_OF_RANGE_ERR, "knitx_stack_dump(): n_values/n_backtrace out of range");
    }
    int rv = KNIT_OK;
    struct knit_str *tmpstr1 = NULL;
    struct knit_str *tmpstr2 = NULL;
    rv = knitx_str_new(knit, &tmpstr1); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_str_new(knit, &tmpstr2);
    if (rv != KNIT_OK)
        goto cleanup_tmpstr1;

    fprintf(stderr, "Stack Values:\n");
    for (int i=stack->vals.len - n_values; i < stack->vals.len; i++) {
        rv = knitx_obj_rep(knit, stack->vals.data[i], tmpstr1, 0);
        if (rv != KNIT_OK)
            goto cleanup_tmpstr2;
        rv = knit_sprintf(knit, tmpstr2, "\ts[%d]: %s\n", i, tmpstr1->str);
        if (rv != KNIT_OK)
            goto cleanup_tmpstr2;
        fprintf(stderr, "%s", tmpstr2->str);
    }
    fprintf(stderr, "Backtrace:\n");
    for (int i=stack->frames.len - n_backtrace; i < stack->frames.len; i++) {
        rv = knitx_frame_rep(knit, &stack->frames.data[i], tmpstr1);
        if (rv != KNIT_OK) 
            goto cleanup_tmpstr2;
        rv = knit_sprintf(knit, tmpstr2, "\tf[%d]: %s\n", i, tmpstr1->str);
        if (rv != KNIT_OK) 
            goto cleanup_tmpstr2;
        fprintf(stderr, "%s", tmpstr2->str);
    }

    knitx_str_destroy(knit, tmpstr2);
    knitx_str_destroy(knit, tmpstr1);
    return KNIT_OK;

cleanup_tmpstr2:
    knitx_str_destroy(knit, tmpstr2);
cleanup_tmpstr1:
    knitx_str_destroy(knit, tmpstr1);
    return rv;
}

//push n nulls to the stack
static int knitx_stack_reserve_values(struct knit *knit, struct knit_stack *stack, int nvalues) {
    knit_assert_h(nvalues >= 0, "");
    struct knit_obj *obj = NULL;
    for (int i=0; i<nvalues; i++) {
        int rv = knit_objp_darray_push(&stack->vals, &obj);
        if (rv != KNIT_OBJP_DARRAY_OK) {
            return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_value: pushing a stack value failed");
        }
    }
    return KNIT_OK;
}

//push a frame for a knit function call
static int knitx_stack_push_frame_for_kcall(struct knit *knit, struct knit_block *block, int nargs, int nexpret) {
    struct knit_frame frm;
    int bsp = knit->ex.stack.vals.len; //base stack pointer
    if (block->nargs != nargs) {
        return knit_error(knit, KNIT_NARGS, "calling a function with the wrong number of arguments, expected %d, called with %d", block->nargs, nargs);
    }
    int rv = knitx_frame_init_kf(knit, &frm, block, 0, bsp, nargs, nexpret);
    if (rv != KNIT_OK) {
        return rv;
    }
    struct knit_exec_state *exs = &knit->ex;
    rv = knit_frame_darray_push(&exs->stack.frames, &frm);
    if (rv != KNIT_FRAME_DARRAY_OK) {
        knitx_frame_deinit(knit, &frm);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_push_frame_for_call(): pushing a stack frame failed");
    }
    return knitx_stack_reserve_values(knit, &exs->stack, block->nlocals);
}

//push a frame for a c function call
static int knitx_stack_push_frame_for_ccall(struct knit *knit, struct knit_cfunc *cfunc, int nargs, int nexpret) {
    struct knit_frame frm;
    int bsp = knit->ex.stack.vals.len; //base stack pointer
    int rv = knitx_frame_init_cf(knit, &frm, cfunc, bsp, nargs, nexpret);
    if (rv != KNIT_OK) {
        return rv;
    }
    struct knit_exec_state *exs = &knit->ex;
    rv = knit_frame_darray_push(&exs->stack.frames, &frm);
    if (rv != KNIT_FRAME_DARRAY_OK) {
        knitx_frame_deinit(knit, &frm);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_push_frame_for_call(): pushing a stack frame failed");
    }
    return KNIT_OK;
}

/* moves the last n stack values to a different index
 * example:
 *     before: 'a' 'b' 'c' 'd'
 *     calling knitx_stack_moveup(knit, stack, 1, 2) result in:
 *     after: 'a' 'c' 'd'
 */
static int knitx_stack_moveup(struct knit *knit, struct knit_stack *stack, int dest_idx, int n) {
    knit_assert_h(dest_idx >= 0, "");
    knit_assert_h((stack->vals.len - n) >= dest_idx, "");
    memmove(stack->vals.data + dest_idx, stack->vals.data + stack->vals.len - n, sizeof(struct knit_obj *) * n);
#ifdef KNIT_CHECKS
    int last_arg = dest_idx + n;
    int nunused = stack->vals.len - last_arg;
    memset(stack->vals.data + last_arg, 0, sizeof(struct knit_obj *) * nunused);
#endif
    stack->vals.len = dest_idx + n; //pops all unneeded values
    return KNIT_OK;
}

static int knitx_stack_pop_frame(struct knit *knit, struct knit_stack *stack) {
    if (stack->frames.len <= 0) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_pop_frame(): attempting to pop an empty stack");
    }
    knitx_frame_deinit(knit, stack->frames.data + stack->frames.len - 1);
    stack->frames.len--;
    return KNIT_OK;
}

static int knitx_exec_state_init(struct knit *knit, struct knit_exec_state *exs) {
    int rv = knit_vars_hasht_init_with_udata(&exs->global_ht, 32, knit);
    if (rv != KNIT_VARS_HASHT_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize vars hashtable");;
    }
    rv = knitx_stack_init(knit, &exs->stack);
    if (rv != KNIT_OK)
        goto cleanup_vars_ht;
    if ((rv = knit_heap_init(knit, &exs->heap, 32000)) != KNIT_OK) {
        goto cleanup_stack;
    }
    return KNIT_OK;
cleanup_stack:
    knitx_stack_deinit(knit, &exs->stack);
cleanup_vars_ht:
    knit_vars_hasht_deinit(&exs->global_ht);
    return rv;
}

static int knitx_exec_state_deinit(struct knit *knit, struct knit_exec_state *exs) {
    knit_vars_hasht_deinit(&exs->global_ht);
    int rv = knitx_stack_deinit(knit, &exs->stack);
    knit_heap_deinit(knit, &exs->heap);
    return rv;
}

/*END OF EXECUTION STATE FUNCS*/

/*
    {A} means 0 or more As, and [A] means an optional A.

    program -> block
	block -> {stat}
    stat -> functioncall ';' |
            assignment ';' 

    assignment -> prefixexp '=' exp

    prefixexp  -> Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name 

    atom_exp -> 'null' | 'false' | 'true' | Numeral | LiteralString | LiteralList | LiteralDict

    exp  ->  atom_exp | prefixexp | functioncall |  ‘(’ exp ‘)’ | exp binop exp | unop exp | function_definition

    functioncall -> prefixexp functioncall_list

    functioncall_list -> '(' ')' |
                         '(' functioncall_args ')'
    functioncall_args -> exp |
                         functioncall_args ',' exp

    function_definition -> 'function' function_prototype function_body
    function_prototype -> '(' [arg_list] ')' 
    arg_list -> Name | arg_list ',' 'Name'
    function_body -> '{' block '}'

    LiteralString ::= "[^"]+"
    Name          ::=  [a-zA-Z_][a-zA-Z0-9_]*
    explist ::= exp {‘,’ exp}
    LiteralList   ::= '[' explist [','] ']' 
    keyval ::= exp ':' exp
    keylist ::= keyval {‘,’ keyval}
    LiteralDict   ::= '{' keylist [','] '}' 
	
    
	var ::=  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name 


	binop ::=  ‘+’ | ‘-’ | ‘*’ | ‘/’ | ‘//’ | ‘^’ | ‘%’ | 
		 ‘&’ | ‘~’ | ‘|’ | ‘>>’ | ‘<<’ | ‘..’ | 
		 ‘<’ | ‘<=’ | ‘>’ | ‘>=’ | ‘==’ | ‘!=’ | 
		 and | or | not

	unop ::= ‘-’ | not | ‘#’ | ‘~’


    symbol          first
    program         Name 
    stat            Name 
    prefixexp       Name 
    exp             null + false + Numeral + LiteralString + '[' + '(' + Name + first(unop)
    literallist     '['

 */

static int knitx_lexer_add_tok(struct knit *knit,
                                struct knit_lex *lxr,
                                int toktype,
                                int offset,
                                int len,
                                int lineno,
                                int colno,
                                void *data) 
{
    struct knit_tok tok;
    tok.toktype = toktype;
    tok.offset = offset;
    tok.lineno = lineno;
    tok.colno = colno;
    tok.len = len;
    if (tok.toktype == KAT_INTLITERAL) {
        knit_assert_h(!!data, "knitx_lexer_add_tok(): adding int literal with no data");
        tok.data.integer = *((int *) data);
    }
    else {
        knit_assert_h(!data, "knitx_lexer_add_tok(): adding token with data argument that is unused");
    }
    int rv = tok_darray_push(&lxr->tokens, &tok);
    if (rv != TOK_DARRAY_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't push token to tokens buffer");
    }
    return KNIT_OK;
}

/*LEXICAL ANALYSIS FUNCS*/

static int knitx_lexer_add_strliteral(struct knit *knit, struct knit_lex *lxr) {
    int beg = lxr->offset;
    char delm = lxr->input->str[beg];
    knit_assert_h(delm == '\'' || delm == '"', "unexpected delimiter");

    lxr->offset++; //skip '
    while (lxr->offset < lxr->input->len && lxr->input->str[lxr->offset] != delm) {
        char *cur = lxr->input->str + lxr->offset;
        if (*cur == '\\') {
            lxr->offset += 2;
            //TODO fix, replace escape seq by single char
        }
        else {
            lxr->offset++;
        }
    }
    if (lxr->offset >= lxr->input->len) {
        return knit_error(knit, KNIT_SYNTAX_ERR, "unterminated string literal");
    }
    lxr->offset++; //skip '
    int rv = knitx_lexer_add_tok(knit, lxr, KAT_STRLITERAL, beg, lxr->offset - beg, lxr->lineno, lxr->colno, NULL);
    return rv;
}

static int knitx_lexer_add_intliteral(struct knit *knit, struct knit_lex *lxr) {
    //TODO check overflow
    //TODO support 0b and 0x prefixes
    int num = 0;
    int beg = lxr->offset;
    while (lxr->offset < lxr->input->len && lxr->input->str[lxr->offset] >= '0' && lxr->input->str[lxr->offset] <= '9') {
        char *cur = lxr->input->str + lxr->offset;
        num = num * 10 + (lxr->input->str[lxr->offset] - '0');
        lxr->offset++;
    }
    return knitx_lexer_add_tok(knit, lxr, KAT_INTLITERAL, beg, lxr->offset - beg, lxr->lineno, lxr->colno, &num);
}

static int knitx_lexer_add_keyword_or_var(struct knit *knit, struct knit_lex *lxr) {
    knit_assert_h((lxr->input->str[lxr->offset] >= 'a' && lxr->input->str[lxr->offset] <= 'z') ||
                  (lxr->input->str[lxr->offset] >= 'A' && lxr->input->str[lxr->offset] <= 'Z') ||
                  lxr->input->str[lxr->offset] == '_', "");
    int beg = lxr->offset;
    while (lxr->offset < lxr->input->len && 
                ((lxr->input->str[lxr->offset] >= 'a' && lxr->input->str[lxr->offset] <= 'z') || 
                 (lxr->input->str[lxr->offset] >= 'A' && lxr->input->str[lxr->offset] <= 'Z') ||
                 (lxr->input->str[lxr->offset] >= '0' && lxr->input->str[lxr->offset] <= '9') ||
                 lxr->input->str[lxr->offset] == '_'                                               ))
    {
        lxr->offset++;
    }
    int len = lxr->offset - beg;
    int type = KAT_VAR; //assume it is a variable name
    if (len == 8 && knit_strl_eq(lxr->input->str + beg, "function", 8)) {
        type = KAT_FUNCTION;
    }
    else if (len == 6 && knit_strl_eq(lxr->input->str + beg, "return", 6)) {
        type = KAT_RETURN;
    }
    else if (len == 5 && knit_strl_eq(lxr->input->str + beg, "false", 5)) {
        type = KAT_FALSE;
    }
    else if (len == 5 && knit_strl_eq(lxr->input->str + beg, "while", 5)) {
        type = KAT_WHILE;
    }
    else if (len == 4 && knit_strl_eq(lxr->input->str + beg, "true", 4)) {
        type = KAT_TRUE;
    }
    else if (len == 4 && knit_strl_eq(lxr->input->str + beg, "null", 4)) {
        type = KAT_NULL;
    }
    else if (len == 4 && knit_strl_eq(lxr->input->str + beg, "else", 4)) {
        type = KAT_ELSE;
    }
    else if (len == 3 && knit_strl_eq(lxr->input->str + beg, "and", 3)) {
        type = KAT_LAND;
    }
    else if (len == 3 && knit_strl_eq(lxr->input->str + beg, "for", 3)) {
        type = KAT_FOR;
    }
    else if (len == 2 && knit_strl_eq(lxr->input->str + beg, "or", 2)) {
        type = KAT_LOR;
    }
    else if (len == 2 && knit_strl_eq(lxr->input->str + beg, "if", 2)) {
        type = KAT_IF;
    }
    return knitx_lexer_add_tok(knit, lxr, type, beg, len, lxr->lineno, lxr->colno, NULL);
}

static int knitx_lexer_skip_wspace(struct knit *knit, struct knit_lex *lxr) {
    while (lxr->offset < lxr->input->len) 
    {
        int c = lxr->input->str[lxr->offset];
        if (!(c == ' '  || c == '\t' || c == '\n' ||
             c == '\r' || c == '\v' || c == '\f'   ))
        {
            break;
        }
        lxr->offset++;
        if (c == '\n')
            lxr->lineno++;
    }
    return KNIT_OK;
}

static int knitx_lexer_skip_comment(struct knit *knit, struct knit_lex *lxr) {
    while (lxr->offset < lxr->input->len && lxr->input->str[lxr->offset] != '\n')
        lxr->offset++;
    return KNIT_OK;
}

static int knitx_lexer_lex(struct knit *knit, struct knit_lex *lxr) {
    int rv = KNIT_OK;
    int nextchar = 0;
again:
    if (lxr->offset >= lxr->input->len) {
        if (!lxr->tokens.len || lxr->tokens.data[lxr->tokens.len-1].toktype != KAT_EOF) {
            return knitx_lexer_add_tok(knit, lxr, KAT_EOF, lxr->offset, 0, lxr->lineno, lxr->colno, NULL);
        }
        return KNIT_OK;
    }
    if (lxr->input->len - lxr->offset > 1)
        nextchar = lxr->input->str[lxr->offset + 1];
    else
        nextchar = 0;
    switch (lxr->input->str[lxr->offset]) {
        case '#':   knitx_lexer_skip_comment(knit, lxr); goto again; break;
        case ';':   rv = knitx_lexer_add_tok(knit, lxr, KAT_SEMICOLON, lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ':':   rv = knitx_lexer_add_tok(knit, lxr, KAT_COLON,     lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '.':   rv = knitx_lexer_add_tok(knit, lxr, KAT_DOT,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ',':   rv = knitx_lexer_add_tok(knit, lxr, KAT_COMMA,     lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '!':   
                    if (nextchar == '=') { //!=
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_NEQ, lxr->offset, 2, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 2;
                    }
                    else {
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OPU_NOT, lxr->offset, 1, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 1;
                    }
                    break;
        case '=':
                    if (nextchar == '=') { //==
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_EQ, lxr->offset, 2, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 2;
                    }
                    else { //=
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_ASSIGN,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 1; break;
                    }
                    break;
        case '>':
                    if (nextchar == '=') { //>=
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_GTEQ, lxr->offset, 2, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 2;
                    }
                    else { //>
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_GT,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 1; break;
                    }
                    break;
        case '<':
                    if (nextchar == '=') { //<=
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_LTEQ, lxr->offset, 2, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 2;
                    }
                    else { //<
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_OP_LT,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL);
                        lxr->offset += 1; break;
                    }
                    break;
        case '[':   rv = knitx_lexer_add_tok(knit, lxr, KAT_OBRACKET,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd++; break;
        case ']':   rv = knitx_lexer_add_tok(knit, lxr, KAT_CBRACKET,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd--; break;
        case '{':   rv = knitx_lexer_add_tok(knit, lxr, KAT_OCURLY,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd++; break;
        case '}':   rv = knitx_lexer_add_tok(knit, lxr, KAT_CCURLY,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd--; break;
        case '(':   rv = knitx_lexer_add_tok(knit, lxr, KAT_OPAREN,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd++; break;
        case ')':   rv = knitx_lexer_add_tok(knit, lxr, KAT_CPAREN,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; lxr->pbcd--; break;
        case '+':   rv = knitx_lexer_add_tok(knit, lxr, KAT_ADD,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '-':   rv = knitx_lexer_add_tok(knit, lxr, KAT_SUB,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '*':   rv = knitx_lexer_add_tok(knit, lxr, KAT_MUL,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '/':   rv = knitx_lexer_add_tok(knit, lxr, KAT_DIV,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '%':   rv = knitx_lexer_add_tok(knit, lxr, KAT_MOD,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '\'':  rv = knitx_lexer_add_strliteral(knit, lxr); break;
        case '"':  rv = knitx_lexer_add_strliteral(knit, lxr); break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': /* FALLTHROUGH */
                    rv = knitx_lexer_add_intliteral(knit, lxr); break;
        case ' ': case '\t': case '\v': case '\f': case '\r': /* FALLTHROUGH */
                    rv = knitx_lexer_skip_wspace(knit, lxr); goto again; break;
        case '\n': 
                    if (lxr->pbcd == 0) {
                        rv = knitx_lexer_add_tok(knit, lxr, KAT_NEWLINE,   lxr->offset, 1, lxr->lineno, lxr->colno, NULL); 
                    }
                    lxr->lineno++;
                    lxr->offset += 1; 
                    if (lxr->pbcd != 0)
                        goto again;
                    break;
        /*abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_*/
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
        case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p':
        case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':
        case 'y': case 'z': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V':
        case 'W': case 'X': case 'Y': case 'Z': case '_': /* FALLTHROUGH */
                    rv = knitx_lexer_add_keyword_or_var(knit, lxr); break;
        default:
            return knit_error(knit, KNIT_SYNTAX_ERR, "unknown character encountered: %c (%u)",
                                                      lxr->input->str[lxr->offset],
                                                      (unsigned)lxr->input->str[lxr->offset]);
    }
    return rv;
}

//skip token
static int knitx_lexer_skip(struct knit *knit, struct knit_lex *lxr) {
    int rv = KNIT_OK;
    knit_assert_h(lxr->tokno < lxr->tokens.len, "knitx_lexer_skip(): invalid .tokno");
    knit_assert_h(lxr->tokno < lxr->tokens.len, "");
    knit_assert_h(lxr->tokens.data[lxr->tokno].toktype != KAT_EOF, "attempting to advance past EOF");
    lxr->tokno++;

    if (lxr->tokno >= lxr->tokens.len) {
        rv = knitx_lexer_lex(knit, lxr);
        if (rv != KNIT_OK)
            return rv;
    }
    return KNIT_OK;
}

static int knitx_lexer_peek_cur(struct knit *knit, struct knit_lex *lxr, struct knit_tok ** tokp) {
    int rv = KNIT_OK;
    *tokp = NULL;
    if (lxr->tokno >= lxr->tokens.len) {
        rv = knitx_lexer_lex(knit, lxr);
        if (rv != KNIT_OK)
            return rv;
    }
    knit_assert_h(lxr->tokno < lxr->tokens.len, "");
    *tokp = &lxr->tokens.data[lxr->tokno];
    return KNIT_OK;
}

static int knitx_lexer_peek_la(struct knit *knit, struct knit_lex *lxr, struct knit_tok **tokp) {
    for (int i=0; i<2; i++) {
        if (lxr->tokno < lxr->tokens.len && lxr->tokens.data[lxr->tokno].toktype == KAT_EOF) {
            *tokp = &lxr->tokens.data[lxr->tokno];
            return KNIT_OK;
        }
        if (lxr->tokno + 1 >= lxr->tokens.len) {
            int rv = knitx_lexer_lex(knit, lxr);
            if (rv != KNIT_OK) {
                *tokp = NULL;
                return rv;
            }
        }
    }
    knit_assert_h(lxr->tokno < lxr->tokens.len, "");
    *tokp = &lxr->tokens.data[lxr->tokno];
    return KNIT_OK;
}

static int knitx_lexer_curtoktype(struct knit *knit, struct knit_prs *prs) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_cur(knit, lxr, &tokp);
    if (rv != KNIT_OK)
        return KAT_EOF;
    return tokp->toktype;
}

static int knitx_lexer_latoktype(struct knit *knit, struct knit_prs *prs) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_la(knit, lxr, &tokp);
    if (rv != KNIT_OK)
        return KAT_EOF;
    return tokp->toktype;
}

//returns 1 if true, 0 if false
static int knitx_lexer_matches_cur(struct knit *knit, struct knit_prs *prs, int toktype) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_cur(knit, lxr, &tokp);
    if (rv != KNIT_OK)
        return 0;
    return tokp->toktype == toktype;
}

//returns 1 if true, 0 if false
static int knitx_lexer_matches_next(struct knit *knit, struct knit_prs *prs, int toktype) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_la(knit, lxr, &tokp);
    if (rv != KNIT_OK)
        return 0;
    return tokp->toktype == toktype;
}

//outi_str = token, outi_str must be initialized
//a copy is made
static int knitx_tok_extract_to_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *outi_str) {
    knit_assert_h(!!tok && !!outi_str, "");
    knit_assert_h(tok->offset < lxr->input->len, "");
    int rv = knitx_str_strlcpy(knit, outi_str, lxr->input->str + tok->offset, tok->len);
    return rv;
}

static const char *knit_knit_tok_name(int name) {
    struct toktype {
        const int type;
        const char * const rep;
    };
    static const struct toktype toktypes[] = {
        {KAT_EOF, "KAT_EOF"},
        {KAT_BOF, "KAT_BOF"}, 
        {KAT_FUNCTION, "KAT_FUNCTION"},
        {KAT_RETURN, "KAT_RETURN"},
        {KAT_TRUE,  "KAT_TRUE"},
        {KAT_FALSE, "KAT_FALSE"},
        {KAT_IF,    "KAT_IF"},
        {KAT_ELSE,  "KAT_ELSE"},
        {KAT_FOR,   "KAT_FOR"},
        {KAT_WHILE, "KAT_WHILE"},
        {KAT_LAND, "KAT_LAND"},
        {KAT_LOR,  "KAT_LOR"},
        {KAT_NULL,  "KAT_NULL"},
        {KAT_STRLITERAL, "KAT_STRLITERAL"},
        {KAT_INTLITERAL, "KAT_INTLITERAL"},
        {KAT_VAR, "KAT_VAR"},
        {KAT_DOT, "KAT_DOT"},
        {KAT_ASSIGN, "KAT_ASSIGN"},
        {KAT_OPAREN, "KAT_OPAREN"},
        {KAT_CPAREN, "KAT_CPAREN"},
        {KAT_ADD, "KAT_ADD"},
        {KAT_SUB, "KAT_SUB"},
        {KAT_MUL, "KAT_MUL"},
        {KAT_DIV, "KAT_DIV"},
        {KAT_MOD, "KAT_MOD"},
        {KAT_OBRACKET, "KAT_OBRACKET"},
        {KAT_CBRACKET, "KAT_CBRACKET"},
        {KAT_OCURLY, "KAT_OCURLY"},
        {KAT_CCURLY, "KAT_CCURLY"},
        {KAT_OP_EQ,     "KAT_OP_EQ"},
        {KAT_OP_NEQ,    "KAT_OP_NEQ"},
        {KAT_OP_GT,     "KAT_OP_GT"},
        {KAT_OP_LT,     "KAT_OP_LT"},
        {KAT_OP_GTEQ,   "KAT_OP_GTEQ"},
        {KAT_OP_LTEQ,   "KAT_OP_LTEQ"},

        {KAT_OPU_NOT,     "KAT_OPU_NOT"},

        {KAT_COMMA, "KAT_COMMA"},
        {KAT_COLON, "KAT_COLON"},
        {KAT_SEMICOLON, "KAT_SEMICOLON"},
        {KAT_NEWLINE, "KAT_NEWLINE"},
        {0, NULL},
    };
    for (int i=0; toktypes[i].rep; i++) {
        if (toktypes[i].type == name) {
            return toktypes[i].rep;
        }
    }
    return NULL;
}

static int knitx_knit_tok_repr_set_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *str) {
    //:'<,'>s/\v(\w+),/{\1, "\1"},/g
    if (tok->toktype == KAT_EOF) {
        return knitx_str_strcpy(knit, str, knit_knit_tok_name(tok->toktype));
    }
    knit_assert_h(!!tok && !!str, "");
    knit_assert_h(tok->offset < lxr->input->len, "");
    struct knit_str *tmp0 = NULL;
    struct knit_str *tmp1 = NULL;
    int rv = knitx_str_new(knit, &tmp0);
    if (rv != KNIT_OK) 
        goto end;
    rv = knitx_str_new(knit, &tmp1);
    if (rv != KNIT_OK)
        goto cleanup_tmp0;
    rv = knitx_tok_extract_to_str(knit, lxr, tok, tmp0);
    if (rv != KNIT_OK)
        goto cleanup_tmp1;
    const char *typestr = knit_knit_tok_name(tok->toktype);
    if (!typestr)
        typestr = "UNKNOWN";

    rv = knit_sprintf(knit, tmp1, "'%s' : %s", tmp0->str, typestr);
    if (rv != KNIT_OK)
        goto cleanup_tmp1;
    rv = knitx_str_strcpy(knit, str, tmp1->str);
    if (rv != KNIT_OK)
        goto cleanup_tmp1;

    knitx_str_destroy(knit, tmp0);
    knitx_str_destroy(knit, tmp1);
    
    return KNIT_OK;
cleanup_tmp1:
    knitx_str_destroy(knit, tmp1);
cleanup_tmp0:
    knitx_str_destroy(knit, tmp0);
end:
    return rv;
}

static int knitx_lexdump(struct knit *knit, struct knit_lex *lxr) {
    if (lxr->input->str)
        fprintf(stderr, "Input: '''\n%s\n'''\n", lxr->input->str);
    struct knit_tok *tok;
    struct knit_str *tokstr;
    int rv = knitx_str_new(knit, &tokstr);
    if (rv != KNIT_OK)
        return rv;
    fprintf(stderr, "Tokens:\n");
    while ((rv = knitx_lexer_peek_cur(knit, lxr, &tok)) == KNIT_OK) {
        if (tok->toktype == KAT_EOF)
            break;
        knitx_knit_tok_repr_set_str(knit, lxr, tok, tokstr);
        fprintf(stderr, "\tToken %d: %s\n", lxr->tokno, tokstr->str);
        rv = knitx_lexer_skip(knit, lxr);
        if (rv != KNIT_OK)
            break;
    }
    knitx_str_destroy(knit, tokstr);
    return rv;
}

/*END OF LEXICAL ANALYSIS FUNCS*/
/*PARSING FUNCS*/

static int knit_cur_block_new(struct knit *knit, struct knit_curblk **curblkp) {
    *curblkp = NULL;
    void *p;
    int rv = knitx_tmalloc(knit, sizeof(struct knit_curblk), &p); 
    if (rv != KNIT_OK)
        return rv;
    struct knit_curblk *nblk = p;
    memset(nblk, 0, sizeof(*nblk));
    nblk->parent = NULL;
    rv = knitx_block_init(knit, &nblk->block);     
    if (rv != KNIT_OK)
        return rv;
    rv = knit_varname_darray_init(&nblk->locals, 8); 
    if (rv != KNIT_OK)
        return rv;
    *curblkp = nblk;;
    return KNIT_OK;
}

//destroy ONLY the curblk, it doesn't try to destroy its parent.
static int knit_cur_block_destroy(struct knit *knit, struct knit_curblk *curblk) {
    //TODO free other resources held by curblk 
    knitx_block_deinit(knit, &curblk->block);
    for (int i=0; i<curblk->locals.len; i++) {
        knitx_str_deinit(knit, &curblk->locals.data[i].name);
    }
    knit_varname_darray_deinit(&curblk->locals);
    knitx_tfree(knit, curblk);
    return KNIT_OK;
}

//assumes sets the new node's next to the current *outp pointer, so, at the beginning it must be NULL
static int knit_patch_loc_new_or_insert(struct knit *knit, int instruction_address, struct knit_patch_list **outp) {
    struct knit_patch_list *node = NULL;
    void *p;
    int rv = knitx_rmalloc(knit, sizeof *node, &p); 
    if (rv != KNIT_OK)
        return rv;
    node = p;
    node->next = *outp;
    node->instruction_address = instruction_address;
    *outp = node;
    return KNIT_OK;
}

//destroys the whole list
static int knit_patch_loc_list_destroy(struct knit *knit, struct knit_patch_list **outp) {
    struct knit_patch_list *node = *outp;
    struct knit_patch_list *next = NULL;
    *outp = NULL;
    while (node) {
        next = node->next;
        knitx_rfree(knit, node);
        node = next;
    }
    return KNIT_OK;
}

static int knit_patch_loc_list_patch_and_destroy(struct knit *knit, struct knit_block *block, struct knit_patch_list **outp, int address) {
    struct knit_patch_list *node = *outp;
    knit_assert_h(address < KINSN_ADDR_UNK && address >= 0, "");
    knit_assert_h(block->insns.len < KINSN_ADDR_UNK, "");
    while (node) {
        struct knit_insn *insn = block->insns.data + node->instruction_address;
        knit_assert_h(node->instruction_address <= block->insns.len, ""); //<= because we assume there will be a next insn
        knit_assert_h(insn->op1 == KINSN_ADDR_UNK, "");
        insn->op1 = address;
        node = node->next;
    }
    knit_patch_loc_list_destroy(knit, outp);
    return KNIT_OK;
}

//should do prs init then do lex init (broken?)
//NOTE lexer must be initialized after this
static int knitx_prs_init1(struct knit *knit, struct knit_prs *prs) {
    memset(prs, 0, sizeof(*prs));
    int rv = knit_cur_block_new(knit, &prs->curblk); 
    if (rv != KNIT_OK)
        return rv;
    return KNIT_OK;
}

static int knitx_prs_deinit(struct knit *knit, struct knit_prs *prs) {
    while (prs->curblk) {
        struct knit_curblk *parent = prs->curblk->parent;
        knit_cur_block_destroy(knit, prs->curblk);
        prs->curblk = parent;
    }
    return KNIT_OK;
}

static int knit_error_expected(struct knit *knit, struct knit_prs *prs, const char *expected_what, const char *msg) {
    struct knit_str str1;
    int rv = knitx_str_init(knit, &str1); 
    if (rv != KNIT_OK)
        return rv;
    struct knit_tok *tok;
    rv = knitx_lexer_peek_cur(knit, &prs->lex, &tok); 
    if (rv != KNIT_OK)
        goto fail_1;
    rv = knitx_knit_tok_repr_set_str(knit, &prs->lex, tok, &str1); 
    if (rv != KNIT_OK) 
        goto fail_1;
    rv = knit_error(knit, KNIT_SYNTAX_ERR, "line %d:%d expected %s near %s", prs->lex.lineno,
                                                                            prs->lex.colno,
                                                                            expected_what,
                                                                            str1.str);
fail_1: knitx_str_deinit(knit, &str1);
    return rv;
}

//returns NULL on error
static struct knit_tok *knitx_lexer_curtok(struct knit *knit, struct knit_prs *prs) {
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_cur(knit, &prs->lex, &tokp);
    if (rv != KNIT_OK) {
        return NULL;
    }
    return tokp;
}

static struct knit_tok *knitx_lexer_latok(struct knit *knit, struct knit_prs *prs) {
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_la(knit, &prs->lex, &tokp);
    if (rv != KNIT_OK) {
        return NULL;
    }
    return tokp;
}

//these macros depend on (knit and prs) being available in lexical scope
#define K_TOKEN_TYPE()    (knitx_lexer_curtoktype(knit, prs))
#define K_LA_TOKEN_TYPE()      (knitx_lexer_latoktype(knit, prs))

//return true if current/lookahead token matches the token type t
#define K_TOKEN_MATCHES(t)   (knitx_lexer_curtoktype(knit, prs) == (t))
#define K_LA_TOKEN_MATCHES(t)     (knitx_lexer_latoktype(knit, prs) == (t))
//return a pointer to current token / lookahead token or NULL
#define K_TOKEN()        (knitx_lexer_curtok(knit, prs))
#define K_LA_TOKEN()          (knitx_lexer_latok(knit, prs))

static int knitx_statement(struct knit *knit) {
    return KNIT_OK;
}

static int knitx_block(struct knit *knit) {
    /* while (knitx_statement */
    return KNIT_OK;
}

enum KAT_IS {
    KAT_IS_BINOP        = 1U << 0,
    KAT_IS_LEFT_ASSOC   = 1U << 1,
    KAT_IS_RIGHT_ASSOC  = 1U << 2,
};
struct ktokinfo {
    int toktype;
    int info;
    int prec;
};
//order is tied to knit_tok_info_idx()
static const struct ktokinfo ktokinfo[] = {
    {KAT_LOR,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  1},
    {KAT_LAND,    KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  2},
    {KAT_OP_EQ,   KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  3},
    {KAT_OP_NEQ,  KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  3},
    {KAT_OP_GT,   KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  4},
    {KAT_OP_LT,   KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  4},
    {KAT_OP_GTEQ, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  4},
    {KAT_OP_LTEQ, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  4},
    {KAT_ADD,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  5},
    {KAT_SUB,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  5},
    {KAT_MUL,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  6},
    {KAT_DIV,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  6},
    {KAT_MOD,     KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  6},
    //code assumes minimum is 1
};
#define NOPINFO (sizeof ktokinfo / sizeof ktokinfo[0])
//returns an index to ktokinfo
static inline int knit_tok_info_idx(int toktype) {
    for (int i=0; i<(int)NOPINFO; i++) {
        if (toktype == ktokinfo[i].toktype)
            return i;
    }
    return -1;
}

static const char *knit_insn_name(int insntype) {
    knit_assert_h(KINSN_TVALID(insntype), "invalid insn");
    return knit_insninfo[insntype].rep;
}

//integer
static inline int knit_tok_prec(int toktype) {
    int i = knit_tok_info_idx(toktype);
#ifdef KNIT_CHECKS
    if (i < 0)
        knit_fatal("invalid toktype");
#endif
    return ktokinfo[i].prec;
}

//boolean
static inline int knit_tok_is(int toktype, int iswhat) {
    int i = knit_tok_info_idx(toktype);
    if (i < 0)
        return 0;
    return (ktokinfo[i].info & iswhat) == iswhat;
}

/*
 *
 *
 * execution plan, and relationship with parsing and lexing
 * suppose we have the expression:
 *    expr = 23 + (8 - 5)
 *               
 *              +
 *          23     -
 *                8  5
 *
 Parsing
        assuming doing things the trivial way (no optimization)
           the atom 23 is parsed, then saved to prs.expr as KAX_INT_LITERAL, then saved as lhs = expr
           then the atom (8 - 5) is parsed, in it:
               the atom 8 is parsed, then saved to prs.expr as KAX_INT_LITERAL, then saved as lhs = expr
               the atom 5 is parsed, then saved to prs.expr as KAX_INT_LITERAL then kept at prs.expr
               binoperation() is called to parse (8 - 5)
                   prs.expr is set to KAX_BIN_OP with leafs pointing to 8 and 5
   binoperation() is called to parse  23   -  (8  -  5):
   prs.expr is set to KAX_BIN_OP with leafs pointing to 23, and (8 - 5)'s bin_op

   Evaluation
   
   eval is called with prs.expr which is binop(+, 23, (8  -  5))
   eval is called with 23, 23 is saved to the function's constants, a load is issued from its index
   eval is called with binop(-, 8, 5) 
        eval is called with 8, 8 is saved to the function's constants, a load is issued from its index
        eval is called with 5, 5 is saved to the function's constants, a load is issued from its index
        then a subtraction instruction is emitted, 
            which subs and movs result to [top - 2] and pops 2, effectively pushing the result
    then an addition instruction is emitted, 
        which subs and movs result to [top - 2] and pops 2, effectively pushing the result
    the result of the expression will be at top of the stack

    Local variables
    consider this function
    function fi(n) {
        foo = 2;
        if (n > 0) {
            n = n + 1;
        }
        bar = 3;
    }

    note how statements and local variable definitions are interleaved
    while parsing we'll have a map of local variable names to integers starting from 0
    and at runtime n stack values will be allocated for them


    globals

    at file scope, every variable is a global
    in functions, assignment to variables is local by default, access before assignment makes 
    the variable implictly refer to a global, and no assignment is allowed
    to assign to globals in functions use g.VARNAME


    Shortcircuiting and branches

    suppose we have:
    cond1 = C1 and C2
    cond2 = C3 or C4

    if C1 is false then we can't evaluate C2, if it is true  then we evaluate C2

    insns will look like:
    L1:
        TEST C1
        JMPFALSE L3
        TEST C2
        JMPFALSE L3
        JMP L4
        EMIT_TRUE
        -----
    L3:
        EMIT_FALSE
    L4:                 #rest of expression

    
    in case of "or"
    if C3 is true  then we can't evaluate C4, if it is false then we evaluate C4
    insns will look like
    L1:
        TEST C3
        JMPTRUE L3
        TEST C4
        JMPTRUE L3
        EMIT_FALSE
        JMP L4
        -----
    L3:
        EMIT_TRUE
    L4:                 #rest of expression


    [if statements]

L1:
    if (C1) {
        S1
        S2
        ..
    }
    else {
L2:
        S3
        S4
        ..
    }
L3:

    insns will look like:
L1:
    TEST C1
    JMP_FALSE L2
    Do S1
    Do S2
    JMP  L3
L2:
    Do S3
    Do S4
L3:
        
*/


//box the current expr that is being built in the parser struct into an object in dynamic memory
//NOTE: this does a shallow copy or a "move". some exprs have resources that they own (ex. str)
static int knitx_save_expr(struct knit *knit, struct knit_prs *prs, struct knit_expr **exprp) {
    void *p;
    int rv = knitx_tmalloc(knit, sizeof **exprp, &p); 
    if (rv != KNIT_OK)
        return rv;
    memcpy(p, &prs->curblk->expr, sizeof **exprp);
    *exprp = p;
    return KNIT_OK;
}

static struct knit_varname *knit_get_varname_by_idx(struct knit_curblk *curblk, int vn_idx) {
    return curblk->locals.data + vn_idx;
}

//save a simple expression as a constant object in a block (to be used by functions and loaded with KCLOAD)
static int kexpr_save_constant(struct knit *knit, struct knit_prs *prs,  struct knit_expr *expr, int *index_out) {
    struct knit_obj *obj = NULL;
    int rv = KNIT_OK;
    if (expr->exptype == KAX_LITERAL_INT) {
        struct knit_int *intp;
        rv = knitx_int_new_gcobj(knit, &intp, expr->u.integer); 
        if (rv != KNIT_OK)
            return rv;
        obj = ktobj(intp);
    }
    else if (expr->exptype == KAX_LITERAL_STR) {
        knit_assert_h(!!expr->u.str, "expected str");
        obj = ktobj(expr->u.str);
    }
    else if (expr->exptype == KAX_VAR_REF) {
        struct knit_varname *vn = knit_get_varname_by_idx(prs->curblk, expr->u.varref.varname_idx);
        knit_assert_h(vn->name.len > 0, "expected valid var ref str");
        obj = ktobj(&vn->name);
    }
    else if (expr->exptype == KAX_FUNCTION) {
        obj = ktobj(expr->u.kfunc);
    }
    else {
        knit_assert_h(0, "kexpr_save_constant(): unsupported exptype");
    }
    return knitx_block_add_constant(knit, &prs->curblk->block, obj, index_out);
}

//add a name in the chain of obj_a.obj_b.obj_c.obj_d
//it'd start with prefix_expr = prefix_init(knit, var_ref: obj_a, obj_b)
//then prefix_expr = prefix_add_name(knit, obj_c, prefix_expr)
//prefix_expr = prefix_add_name(knit, obj_d, prefix_expr)
static int knitx_expr_prefix_add_name(struct knit *knit, struct knit_str *name, struct knit_expr *out_expr)
{
    knit_assert_h(!!out_expr->u.prefix.chain, "");
    struct knit_varname_chain *chain = out_expr->u.prefix.chain;
    while (chain->next)
        chain = chain->next;
    void *p = NULL;
    int rv = knitx_tmalloc(knit, sizeof(struct knit_varname_chain), &p); 
    if (rv != KNIT_OK)
        return rv;
    chain->next = p;
    chain->next->name = name;
    chain->next->next = NULL;
    return KNIT_OK;
}

//owns both name and child_name
static int knitx_expr_prefix_init(struct knit *knit, 
                                  struct knit_expr *parent,
                                  struct knit_str *child_name,
                                  struct knit_expr *out_expr)
{
    void *p = NULL;
    int rv = knitx_tmalloc(knit, sizeof(struct knit_varname_chain), &p); 
    if (rv != KNIT_OK)
        return rv;
    out_expr->exptype = KAX_OBJ_DOT;
    out_expr->u.prefix.parent = parent;
    out_expr->u.prefix.chain = p;
    out_expr->u.prefix.chain->next = NULL;
    out_expr->u.prefix.chain->name = child_name;
    return KNIT_OK;
}

//doesn't own name
//possible return values: KNIT_RETRIEVED, KNIT_NOT_FOUND
static int knitx_get_block_var_idx(struct knit *knit, struct knit_curblk *curblk, struct knit_str *name, int *vn_idx) {
    for (int i=0; i<curblk->locals.len; i++) {
        struct knit_varname *vn = curblk->locals.data + i;
        knit_assert_s(vn->name.len > 0, "");
        if (knitx_str_streq(knit, name, &vn->name)) {
            *vn_idx = i;
            return KNIT_RETRIEVED;
        }
    }
    *vn_idx = -1;
    return KNIT_NOT_FOUND;
}

static int knitx_varname_set_location(struct knit *knit, struct knit_curblk *curblk, int vn_idx, int location) {
    struct knit_varname *vn = knit_get_varname_by_idx(curblk, vn_idx);
    knit_assert_s( vn->location == KLOC_UNKNOWN, "");
    if (location == KLOC_LOCAL_VAR) {
        //allocate space for a local variable
        vn->idx = curblk->block.nlocals;
        curblk->block.nlocals++;
    }
    else if (location == KLOC_ARG) {
        vn->idx = curblk->block.nargs;
        curblk->block.nargs++;
    }
    vn->location = location;
    return KNIT_OK;
}

//doesn't own name
//add_location: one of the values in KNIT_VARLOC
//used while parsing funcdefs
static int knitx_add_block_var(struct knit *knit, struct knit_curblk *curblk, struct knit_str *name, int *vn_idx) {
    *vn_idx = -1;
    if (knitx_get_block_var_idx(knit, curblk, name, vn_idx) != KNIT_NOT_FOUND) {
        return KNIT_DUPLICATE_KEY;
    }
    struct knit_varname vn = {0};
    int rv = knitx_str_init_copy(knit, &vn.name, name); 
    if (rv != KNIT_OK)
        return rv;
    vn.location = KLOC_UNKNOWN;
    rv = knit_varname_darray_push(&curblk->locals, &vn);
    if (rv != KNIT_VARNAME_DARRAY_OK) {
        knitx_str_deinit(knit, &vn.name);
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't push local variable to varname array");
    }
    *vn_idx = curblk->locals.len - 1;
    return KNIT_OK;
}

//this is used during parsing, if it finds an existing item it returns its index, otherwise it initializes a new one with the type KLOC_UNKNOWN
//possible return values: KNIT_RETRIEVED : found existing
//                        KNIT_NOT_FOUND : created new
//doesn't own name
static int knitx_get_or_add_block_var(struct knit *knit, struct knit_curblk *curblk, struct knit_str *name, int *vn_idx) {
    int rv = knitx_get_block_var_idx(knit, curblk, name, vn_idx);
    if (rv == KNIT_NOT_FOUND) {
        rv = knitx_add_block_var(knit, curblk, name, vn_idx);
        if (rv != KNIT_OK) {
            return rv;
        }
        return KNIT_NOT_FOUND;
    }
    return rv;
}

static int knitx_skip_nl(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    while (K_TOKEN_MATCHES(KAT_NEWLINE)) {
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
            return rv;
    }
    return rv;
}

//darray is assumed to be initialized and empty
static int kexpr_func_call_arglist(struct knit *knit, struct knit_prs *prs, struct knit_expr_darray *darray) {
    knit_assert_s( K_TOKEN_MATCHES(KAT_OPAREN) , "");
    int rv;
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
        return rv;
    if (K_TOKEN_MATCHES(KAT_CPAREN)) {
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
            return rv;
        return KNIT_OK;
    }
    while (1) {
        rv = knitx_expr(knit, prs);  
        if (rv != KNIT_OK)
            return rv;
        struct knit_expr *arg_expr = NULL;
        rv = knitx_save_expr(knit, prs, &arg_expr);  
        if (rv != KNIT_OK)
            return rv;
        rv = knit_expr_darray_push(darray, &arg_expr);
        if (rv != KNIT_EXPR_DARRAY_OK) {
            return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't push to arg expressions dynamic array"); 
        }
        if (!K_TOKEN_MATCHES(KAT_COMMA))
            break;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) //skip comma
            return rv; 
    }
    if (!K_TOKEN_MATCHES(KAT_CPAREN)) {
        return knit_error_expected(knit, prs, ")", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
        return rv;
    return KNIT_OK;
}

static int knit_expr_list_deinit(struct knit *knit, struct knit_prs *prs, struct knit_expr_darray *darray) {
    for (int i=0; i<darray->len; i++) {
        struct knit_expr *arg_expr = darray->data[i];
        knitx_expr_destroy(knit, prs, arg_expr);
    }
    knit_expr_darray_deinit(darray);
    return KNIT_OK;
}

static int knitx_expr_destroy_prefix(struct knit *knit, struct knit_expr *expr) {
    knit_assert_h(!!expr->u.prefix.chain, "");
    
    return KNIT_OK;
}

static int knitx_expr_destroy(struct knit *knit, struct knit_prs *prs, struct knit_expr *prs_expr) {
    if (prs_expr->exptype == KAX_CALL) {
        knit_expr_list_deinit(knit, prs, &prs_expr->u.call.args);
        knitx_expr_destroy(knit, prs, prs_expr->u.call.called); //how will this be destroyed?, how does it affect the block referenced
    }
    else if (prs_expr->exptype == KAX_LITERAL_LIST) {
        knit_expr_list_deinit(knit, prs, &prs_expr->u.elist);
    }
    knitx_tfree(knit, prs_expr); 
    return KNIT_OK;
}

static int knitx_stmt_deinit(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt) {
    //not implemented
    if (stmt->stmttype == KSTMT_ASSIGN) {
    }
    else if (stmt->stmttype == KSTMT_FOR) {
    }
    else if (stmt->stmttype == KSTMT_WHILE) {
    }
    else if (stmt->stmttype == KSTMT_RETURN) {
    }
    else if (stmt->stmttype == KSTMT_IF) {

    /*
     * L1: if (C) { [false? jmp L2, True? exec block then jmp L3]
     *  S1, S2 ...
     *  jmp L3:
     * }
     * L2: else {
     * }
     * L3:
     *
     */
    }
    else if (stmt->stmttype == KSTMT_EXPR) {
    }
    else if (stmt->stmttype == KSTMT_SBLOCK) {
    }
    else {
        knit_assert_h(0, "unknown stmt type");
    }
    return KNIT_OK;
}

static int kexpr_funcdef(struct knit *knit, struct knit_prs *prs) {
    knit_assert_s(K_TOKEN_MATCHES(KAT_FUNCTION),  "");
    struct knit_curblk *curblk = NULL;
    int rv = knit_cur_block_new(knit, &curblk); 
    if (rv != KNIT_OK)
        return rv;
    curblk->parent = prs->curblk;
    prs->curblk = curblk;
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
        return rv;
    //TODO fix error handling
    
    if (!K_TOKEN_MATCHES(KAT_OPAREN)) {
        return knit_error_expected(knit, prs, "(", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
        return rv;

    while (K_TOKEN_MATCHES(KAT_VAR)) {
        struct knit_str *arg_name = NULL;
        rv = knitx_str_new(knit, &arg_name); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_tok_extract_to_str(knit, &prs->lex, K_TOKEN(), arg_name);  
        if (rv != KNIT_OK)
            return rv;
        int vn_idx = -1;
        if (knitx_str_streqc(knit, arg_name, "g")) {
            return knit_parse_error(prs, "g is a reserved keyword, it is not allowed as an argument name.");
        }
        rv = knitx_add_block_var(knit, curblk, arg_name, &vn_idx); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_varname_set_location(knit, prs->curblk, vn_idx, KLOC_ARG); 
        if (rv != KNIT_OK)
            return rv;
        
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
        if (!K_TOKEN_MATCHES(KAT_COMMA)) {
            break;
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
            return rv;
    }
    if (!K_TOKEN_MATCHES(KAT_CPAREN)) {
        return knit_error_expected(knit, prs, ")", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) 
        return rv;
    if (!K_TOKEN_MATCHES(KAT_OCURLY)) {
        return knit_error_expected(knit, prs, "{", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;

    while (!K_TOKEN_MATCHES(KAT_EOF) && !K_TOKEN_MATCHES(KAT_CCURLY)) {
        rv = knitx_stmt_prs_emit(knit, prs, KSTMT_ALL); 
    }
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_emit_ret(knit, prs, 0); //this can be redundant if the function already has a return stmt
    if (rv != KNIT_OK)
        return rv;

    if (!K_TOKEN_MATCHES(KAT_CCURLY)) {
        return knit_error_expected(knit, prs, "}", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;

    prs->curblk = curblk->parent;
    knit_assert_s(!!prs->curblk,  "");

    void *p;

    rv  = knitx_tmalloc(knit, sizeof(struct knit_kfunc), &p);  
    if (rv != KNIT_OK)
        return rv;

    struct knit_kfunc *kfunc = p;
    kfunc->ktype = KNIT_KFUNC;
    kfunc->block = curblk->block; //move the block itself, assumes no self references in it, takes ownership

#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_curblock_dump(knit, curblk);
        knitx_block_dump(knit, &kfunc->block);
    }
#endif

    /*this destroys everything in curblock except .block itsel, (but what if we need debug info?, it should be optionally saved somewhere)f*/
    knit_varname_darray_deinit(&curblk->locals);
    knitx_tfree(knit, curblk);

    prs->curblk->expr.exptype = KAX_FUNCTION;
    prs->curblk->expr.u.kfunc = kfunc;

    return KNIT_OK;
}

static void knit_kfunc_deinit(struct knit *knit, struct knit_kfunc *kfunc) {
    knitx_block_deinit(knit, &kfunc->block);
}

static void knit_kfunc_destroy(struct knit *knit, struct knit_kfunc *kfunc) {
    knit_kfunc_deinit(knit, kfunc);
    knitx_rfree(knit, kfunc);
}

static int kexpr_prefix(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    knit_assert_s(K_TOKEN_MATCHES(KAT_DOT) || K_TOKEN_MATCHES(KAT_OBRACKET) || K_TOKEN_MATCHES(KAT_OPAREN),  "");
    while ( K_TOKEN_MATCHES(KAT_DOT)      || 
            K_TOKEN_MATCHES(KAT_OBRACKET) ||
            K_TOKEN_MATCHES(KAT_OPAREN)     ) 
    {
        if (K_TOKEN_MATCHES(KAT_DOT)) {
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
            if (!K_TOKEN_MATCHES(KAT_VAR)) {
                return knit_error_expected(knit, prs, "a variable name", "");
            }
            struct knit_expr *rootexpr =  NULL;
            rv = knitx_save_expr(knit, prs, &rootexpr);  
            if (rv != KNIT_OK)
                return rv;
            struct knit_str *child_name = NULL;
            rv = knitx_str_new(knit, &child_name); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_tok_extract_to_str(knit, &prs->lex, K_TOKEN(), child_name);  
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_expr_prefix_init(knit, rootexpr, child_name, &prs->curblk->expr);  
            if (rv != KNIT_OK)
                return rv; 
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
            while (K_TOKEN_MATCHES(KAT_DOT)) {
                if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
                if (!K_TOKEN_MATCHES(KAT_VAR)) {
                    return knit_error_expected(knit, prs, "a variable name", ""); 
                }
                rv = knitx_str_new(knit, &child_name); 
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_tok_extract_to_str(knit, &prs->lex, K_TOKEN(), child_name);  
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_expr_prefix_add_name(knit, child_name, &prs->curblk->expr);  
                if (rv != KNIT_OK)
                    return rv;
                if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
            }
        }
        else if (K_TOKEN_MATCHES(KAT_OPAREN)) {
            //function call
            struct knit_expr *rootexpr =  NULL;
            rv = knitx_save_expr(knit, prs, &rootexpr);  
            if (rv != KNIT_OK)
                return rv;

            struct knit_expr_darray arglist =  {0};
            rv = knit_expr_darray_init(&arglist, 2); 
            if (rv != KNIT_EXPR_DARRAY_OK) {
                return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize arg expressions dynamic array"); 
            }
            rv = kexpr_func_call_arglist(knit, prs, &arglist);  
            if (rv != KNIT_OK)
                return rv;
            struct knit_expr *prs_expr =  &prs->curblk->expr;
            prs_expr->exptype = KAX_CALL;
            prs_expr->u.call.called = rootexpr;
            prs_expr->u.call.args = arglist;
        }
        else if (K_TOKEN_MATCHES(KAT_OBRACKET)) {
            //NOTE: slicing should be supported here in the future
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //[
            struct knit_expr *indexed_expr =  NULL;
            rv = knitx_save_expr(knit, prs, &indexed_expr);  
            if (rv != KNIT_OK)
                return rv;
            struct knit_expr *index_expr =  NULL;
            rv = knitx_expr(knit, prs); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_save_expr(knit, prs, &index_expr);  
            if (rv != KNIT_OK)
                return rv;
            if (!K_TOKEN_MATCHES(KAT_CBRACKET)) {
                return knit_error_expected(knit, prs, "]", ""); 
            }
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //]

            struct knit_expr *prs_expr =  &prs->curblk->expr;
            prs_expr->exptype = KAX_INDEX;
            prs_expr->u.index.indexed = indexed_expr;
            prs_expr->u.index.index = index_expr;
        }
    }
    return KNIT_OK;
}

//parses { key : value, key : value }
static int kexpr_key_value(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    struct knit_expr *key_expr = NULL;
    struct knit_expr *val_expr = NULL;
    rv = knitx_expr(knit, prs);                 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_save_expr(knit, prs, &key_expr); 
    if (rv != KNIT_OK)
        return rv;

    if (!K_TOKEN_MATCHES(KAT_COLON)) {
        return knit_error_expected(knit, prs, ":", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //skip :

    rv = knitx_expr(knit, prs);                 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_save_expr(knit, prs, &val_expr); 
    if (rv != KNIT_OK)
        return rv;
    struct knit_expr *prs_expr = &prs->curblk->expr;
    prs_expr->exptype = KAX_PAIR;
    prs_expr->u.bin.op = KAX_PAIR;
    prs_expr->u.bin.lhs = key_expr;
    prs_expr->u.bin.rhs = val_expr;
    return KNIT_OK;
}

static int kexpr_atom(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    struct knit_expr *prs_expr =  &prs->curblk->expr;
    if (K_TOKEN_MATCHES(KAT_OPU_NOT) || K_TOKEN_MATCHES(KAT_ADD) || K_TOKEN_MATCHES(KAT_SUB)) {
        int op = knitx_lexer_curtoktype(knit, prs);
        struct knit_expr *operand =  NULL;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //skip unary op
        rv = kexpr_atom(knit, prs); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_save_expr(knit, prs, &operand);  
        if (rv != KNIT_OK)
            return rv;
        prs_expr->exptype = KAX_UN_OP;
        prs_expr->u.un.op = op;
        prs_expr->u.un.operand = operand;
    }
    else if (K_TOKEN_MATCHES(KAT_FUNCTION)) {
        rv = kexpr_funcdef(knit, prs);
    }
    else if (K_TOKEN_MATCHES(KAT_INTLITERAL)) {
        prs_expr->exptype = KAX_LITERAL_INT;
        prs_expr->u.integer = K_TOKEN()->data.integer;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_OPAREN)) {
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
        rv = kexpr_expr(knit, prs, 1); 
        if (rv != KNIT_OK)
            return rv;
        if (!K_TOKEN_MATCHES(KAT_CPAREN)) {
            return knit_error_expected(knit, prs, ")", "");
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_VAR)) {
        struct knit_str name;
        rv = knitx_str_init(knit, &name);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_tok_extract_to_str(knit, &prs->lex, K_TOKEN(), &name);  
        if (rv != KNIT_OK)
            return rv;
        if (knitx_str_streqc(knit, &name, "g")) {
            prs_expr->exptype = KAX_G;
            knitx_str_deinit(knit, &name);
        }
        else {
            rv = knitx_get_or_add_block_var(knit, prs->curblk, &name, &prs_expr->u.varref.varname_idx);
            knitx_str_deinit(knit, &name);
            prs_expr->exptype = KAX_VAR_REF;
            if (rv != KNIT_RETRIEVED && rv != KNIT_NOT_FOUND) {
                return rv; 
            }
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_STRLITERAL)) {
        prs_expr->exptype = KAX_LITERAL_STR;
        rv = knitx_str_new_gcobj(knit, &prs_expr->u.str); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_tok_extract_to_str(knit, &prs->lex, K_TOKEN(), prs_expr->u.str); 
        if (rv != KNIT_OK)
            return rv;
        knit_assert_h( (prs_expr->u.str->len >= 2) && 
                        ((prs_expr->u.str->str[0] == '\'' && prs_expr->u.str->str[prs_expr->u.str->len - 1] == '\'') ||
                         (prs_expr->u.str->str[0] == '"' && prs_expr->u.str->str[prs_expr->u.str->len - 1] == '"')   ),
                       "expected quoted string literal");
        rv = knitx_str_mutsubstr(knit, prs_expr->u.str, 1, prs_expr->u.str->len - 1); 
        if (rv != KNIT_OK)
            return rv;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_OBRACKET)) {
	//list literal
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //[

        struct knit_expr_darray explist =  {0};
        rv = knit_expr_darray_init(&explist, 0); 
        if (rv != KNIT_OK)
            return rv;
        while (!K_TOKEN_MATCHES(KAT_CBRACKET)) {
            if (explist.len > 0) {
                if (!K_TOKEN_MATCHES(KAT_COMMA)) {
                    return knit_error_expected(knit, prs, ",", "");
                }
                if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //comma
                //allow trailing comma
                if (K_TOKEN_MATCHES(KAT_CBRACKET))
                    break;
            }
            rv = knitx_expr(knit, prs);  
            if (rv != KNIT_OK)
                return rv;
            struct knit_expr *item_expr = NULL;
            rv = knitx_save_expr(knit, prs, &item_expr);  
            if (rv != KNIT_OK)
                return rv;
            rv = knit_expr_darray_push(&explist, &item_expr); 
            if (rv != KNIT_OK)
                return rv;
        }
        if (!K_TOKEN_MATCHES(KAT_CBRACKET)) {
            return knit_error_expected(knit, prs, "]", "");
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //]
        prs_expr->exptype = KAX_LITERAL_LIST;
        prs_expr->u.elist = explist;
    }
    else if (K_TOKEN_MATCHES(KAT_OCURLY)) {
        //dictionary literal
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //{
        struct knit_expr_darray explist =  {0};
        rv = knit_expr_darray_init(&explist, 0); 
        if (rv != KNIT_OK)
            return rv;
        while (!K_TOKEN_MATCHES(KAT_CCURLY)) {
            if (explist.len > 0) {
                if (!K_TOKEN_MATCHES(KAT_COMMA)) {
                    return knit_error_expected(knit, prs, ",", "");
                }
                if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //comma
                //allow trailing comma
                if (K_TOKEN_MATCHES(KAT_CBRACKET))
                    break;
            }
            rv = kexpr_key_value(knit, prs); 
            if (rv != KNIT_OK)
                return rv;

            struct knit_expr *item_expr = NULL; 
            rv = knitx_save_expr(knit, prs, &item_expr);  
            if (rv != KNIT_OK)
                return rv;
            rv = knit_expr_darray_push(&explist, &item_expr); 
            if (rv != KNIT_OK)
                return rv;
        }
        if (!K_TOKEN_MATCHES(KAT_CCURLY)) {
            return knit_error_expected(knit, prs, "}", "");
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //}
        prs_expr->exptype = KAX_LITERAL_DICT;
        prs_expr->u.elist = explist;
    }
    else if (K_TOKEN_MATCHES(KAT_NULL)) {
        prs_expr->exptype = KAX_LITERAL_NULL;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_TRUE)) {
        prs_expr->exptype = KAX_LITERAL_TRUE;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else if (K_TOKEN_MATCHES(KAT_FALSE)) {
        prs_expr->exptype = KAX_LITERAL_FALSE;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv;
    }
    else {
        return knit_error_expected(knit, prs, "an expression", "");
    }

    if (K_TOKEN_MATCHES(KAT_DOT) || K_TOKEN_MATCHES(KAT_OBRACKET) || /*func call*/K_TOKEN_MATCHES(KAT_OPAREN)) {
        rv = kexpr_prefix(knit, prs);
    }
    return rv;
}

static int knitx_emit_1(struct knit *knit, struct knit_prs *prs, int opcode) {
    knit_assert_h(KINSN_TVALID(opcode), "invalid insn");
    struct knit_insn insn;
    insn.insn_type = opcode;
    insn.op1 = -1;
    int rv = knitx_block_add_insn(knit, &prs->curblk->block, &insn); 
    if (rv != KNIT_OK)
        return rv; 
    return KNIT_OK;
}

static int knitx_emit_2(struct knit *knit, struct knit_prs *prs, int opcode, int arg1) {
    knit_assert_h(KINSN_TVALID(opcode), "invalid insn");
    knit_assert_h(arg1 >= -16000 && arg1 <= 16000, "invalid insn op1");
    struct knit_insn insn;
    insn.insn_type = opcode;
    insn.op1 = arg1;
    int rv = knitx_block_add_insn(knit, &prs->curblk->block, &insn); 
    if (rv != KNIT_OK)
        return rv; 
    return KNIT_OK;
}

enum knit_eval_context {
    KEVAL_VALUE, //pushes on the stack
    KEVAL_BOOLEAN, //in case of boolean expressions it doesn't push, instead uses ex.last_cond
    KEVAL_MCALL, //eval as a method call, this prevents popping the 'self' reference
};

//emit instructions that do assignment, taking in consideration what kind of lhs we have
static int knitx_emit_assignment(struct knit *knit, struct knit_prs *prs, struct knit_expr *lhs, struct knit_expr *rhs) {
    int rv = KNIT_OK;
    if (lhs->exptype == KAX_VAR_REF) {
        knit_assert_s(lhs->u.varref.varname_idx >= 0 && lhs->u.varref.varname_idx < prs->curblk->locals.len,  "");
        int vn_idx = lhs->u.varref.varname_idx;
        struct knit_varname *vn = knit_get_varname_by_idx(prs->curblk, vn_idx);
        struct knit_str *name = &vn->name;

        if (vn->location == KLOC_UNKNOWN) {
            if (knitx_is_in_filescope(knit, prs)) {
                rv = knitx_varname_set_location(knit, prs->curblk, vn_idx, KLOC_GLOBAL_RW); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else {
                rv = knitx_varname_set_location(knit, prs->curblk, vn_idx, KLOC_LOCAL_VAR); 
                if (rv != KNIT_OK)
                    return rv;
            }
        }
        if (vn->location == KLOC_LOCAL_VAR || vn->location == KLOC_ARG)  {
            rv = knitx_emit_expr_eval(knit, prs, rhs, KEVAL_VALUE, 1);  
            if (rv != KNIT_OK)
                return rv; 
            int offset = 0; //stack offset relative to bsp
            if (vn->location == KLOC_LOCAL_VAR) {
                offset = vn->idx;
            }
            else if (vn->location == KLOC_ARG) {
                //this is done because of how the stack is set up, we subtract from bsp(the args are supposed to be there),
                //and we assume the current function is pushed last
                offset = -vn->idx - 2; 
            }
            else {
                return knit_parse_error(prs, "variable '%s' unexpected location", vn->name.str); 
            }
            rv = knitx_emit_2(knit, prs, KLSTORE, offset); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (vn->location == KLOC_GLOBAL_R)  {
            if (knitx_is_in_filescope(knit, prs)) {
                rv = knitx_varname_set_location(knit, prs->curblk, vn_idx, KLOC_GLOBAL_RW); 
                if (rv != KNIT_OK)
                    return rv;
                goto global_write;
            }
            //the variable is an implicit global, it cannot be assigned to as if it was a local
            return knit_parse_error(prs, "variable '%s' is used before assignment", name->str);
        }
        else if (vn->location == KLOC_GLOBAL_RW)  {
            global_write:
            {
                int idx = -1;
                rv = knitx_current_block_add_strl_constant(knit, prs, name->str, name->len, &idx);  
                if (rv != KNIT_OK)
                    return rv; 
                rv = knitx_emit_2(knit, prs, KCLOAD, idx); //load name of global variable
                if (rv != KNIT_OK)
                    return rv; 
                rv = knitx_emit_expr_eval(knit, prs, rhs, KEVAL_VALUE, 1); //evaluate the result of rhs and push it
                if (rv != KNIT_OK)
                    return rv; 
                rv = knitx_emit_1(knit, prs, K_GLB_STORE);
            }
        }
        else {
            knit_assert_s(0,  "unreachable");
        }
    }
    else if (lhs->exptype == KAX_G) {
        return knit_parse_error(prs, "assignment to g is not allowed.");
    }
    else if (lhs->exptype == KAX_OBJ_DOT) {
        struct knit_expr *parent = lhs->u.prefix.parent;
        struct knit_varname_chain *chain = lhs->u.prefix.chain;
        knit_assert_h(parent && chain && chain->name, "invalid KAX_OBJ_DOT object");
        if (parent->exptype == KAX_G) {
            //hardcoded case for trivial global variables (the rest is not implemeneted)
            //globals are accessed by: g.VARNAME
            if (chain->next) {
                return knit_parse_error(prs, "obj.obj style access not implemented");
            }
            struct knit_str *var_name = chain->name;
            int idx = -1;
            //save name of global variable in function constants
            rv = knitx_current_block_add_strl_constant(knit, prs, var_name->str, var_name->len, &idx);  
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_emit_2(knit, prs, KCLOAD, idx); //load name of global variable
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_emit_expr_eval(knit, prs, rhs, KEVAL_VALUE, 1); //evaluate the result of rhs and push it
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_emit_1(knit, prs, K_GLB_STORE);
        }
        else {
            return knit_parse_error(prs, "obj.obj = obj assignment is not implemented");
        }

    }
    else if (lhs->exptype == KAX_INDEX) {
        rv = knitx_emit_expr_eval(knit, prs, lhs->u.index.indexed, KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_expr_eval(knit, prs, lhs->u.index.index,   KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_expr_eval(knit, prs, rhs, KEVAL_VALUE, 1); //evaluate the result of rhs and push it
        if (rv != KNIT_OK)
            return rv; 
        rv = knitx_emit_1(knit, prs, KINDX_SET);  
        if (rv != KNIT_OK)
            return rv;

    }
    else {
        return knit_parse_error(prs, "unexpected lhs type");
    }
    return KNIT_OK;
}

static int knit_is_logical_op_token(int toktype) {
    return  toktype == KAT_LAND || toktype == KAT_LOR;
}

//is a boolean test op
static int knit_is_test_op(int op) {
    return op == KTESTEQ ||   
           op == KTESTNEQ ||  
           op == KTESTGT ||   
           op == KTESTLT ||   
           op == KTESTLTEQ || 
           op == KTESTGTEQ || 
           op == KTEST  ||
           op == KTESTNOT;
}

static int knitx_emit_logical_operation(struct knit *knit,
                                        struct knit_prs *prs,
                                        struct knit_expr *expr,
                                        int eval_ctx)
{
    knit_assert_h(expr != &prs->curblk->expr && expr->exptype == KAX_LOGICAL_BINOP, "");
    int rv = KNIT_OK;
    struct knit_block *block = &prs->curblk->block;

    int nexpected = 0;
    if (eval_ctx == KEVAL_VALUE)
        nexpected = 1;
    knit_assert_h(eval_ctx == KEVAL_VALUE || eval_ctx == KEVAL_BOOLEAN, "code was written with only these in mind");
    //if eval_ctx is KEVAL_BOOLEAN we don't keep the return value
    //otherwise we keep it and test it
    //so that the result of:
    //v = true and 'foo'
    //will be 'foo'
    //v = true and false
    //will be false
    //v = null and 'foo'
    //will be null

    if (expr->u.logic_bin.op == KAT_LAND) {
        rv = knitx_emit_expr_eval(knit, prs, expr->u.logic_bin.lhs, eval_ctx, nexpected); 
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_VALUE) {
            knitx_emit_2(knit, prs, KPUSH, -1); //duplicate and test
            knitx_emit_1(knit, prs, KTEST);
        }
        knitx_emit_2(knit, prs, KJMPFALSE, KINSN_ADDR_UNK);
    }
    else if (expr->u.logic_bin.op == KAT_LOR) {
        rv = knitx_emit_expr_eval(knit, prs, expr->u.logic_bin.lhs, eval_ctx, nexpected); 
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_VALUE) {
            knitx_emit_2(knit, prs, KPUSH, -1);
            knitx_emit_1(knit, prs, KTEST);
        }
        knitx_emit_2(knit, prs, KJMPTRUE, KINSN_ADDR_UNK);
    }
    else {
        return knit_parse_error(prs, "unexpected binary logical operator");
    }

    rv = knit_patch_loc_new_or_insert(knit, block->insns.len - 1, &expr->u.logic_bin.plist); 
    if (rv != KNIT_OK)
        return rv;
    //in case of KEVAL_VALUE and KAT_LAND at this point execution at this point implies the first test succeeded, so it needs to be discarded
    //in case of KEVAL_VALUE and KAT_LOR at this point execution at this point implies the first test failed, so it needs to be discarded
    //otherwise this will be jumped
    if (eval_ctx == KEVAL_VALUE) {
        knitx_emit_2(knit, prs, KPOP, 1);
    }
    rv = knitx_emit_expr_eval(knit, prs, expr->u.logic_bin.rhs, eval_ctx, nexpected); 
    if (rv != KNIT_OK)
        return rv;

    int address = block->insns.len; 
    rv = knit_patch_loc_list_patch_and_destroy(knit, block, &expr->u.logic_bin.plist, address); 
    if (rv != KNIT_OK)
        return rv;

    return KNIT_OK;
}

//the result will be in ex.last_cond
static int knitx_test_bool(struct knit *knit, struct knit_obj *obj) {
    if (obj->u.ktype == KNIT_NULL || obj->u.ktype == KNIT_FALSE)
        knit->ex.last_cond = 0;
    else {
        knit->ex.last_cond = 1;
    }
    return KNIT_OK;
}

//turn an expr into a sequence of bytecode insns that result in its result being at the top of the stack
//nexpected can be an integer or a special KRES_* value
static int knitx_emit_expr_eval(struct knit *knit, struct knit_prs *prs, struct knit_expr *expr, int eval_ctx, int nexpected) {
    knit_assert_h(eval_ctx != KEVAL_BOOLEAN || nexpected == 0, "");
    knit_assert_h(expr != &prs->curblk->expr, "");
    int rv = KNIT_OK;
    if (expr->exptype == KAX_CALL) {
        //push them in reverse, see stack layout (i got them backwards at first!)
        for (int i = expr->u.call.args.len - 1; i >= 0; i--) {
            struct knit_expr *arg_expr = expr->u.call.args.data[i]; 
            rv = knitx_emit_expr_eval(knit, prs, arg_expr, KEVAL_VALUE, 1); 
            if (rv != KNIT_OK)
                return rv;
        }
        int nargs = expr->u.call.args.len;
        if (expr->u.call.called->exptype == KAX_OBJ_DOT) {
            if (expr->u.call.called->u.prefix.parent->exptype == KAX_G) {
                rv = knitx_emit_expr_eval(knit, prs, expr->u.call.called, KEVAL_VALUE, 1); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else {
                //currently any obj.func call is assumed to be am method call, this should probably be fixed
                nargs++;
                rv = knitx_emit_expr_eval(knit, prs, expr->u.call.called, KEVAL_MCALL, 2); 
                if (rv != KNIT_OK)
                    return rv;
            }
        }
        else {
            rv = knitx_emit_expr_eval(knit, prs, expr->u.call.called, KEVAL_VALUE, 1); 
            if (rv != KNIT_OK)
                return rv;
        }

        //TODO at this point the stack will have return values
        //this will be broken if a function returns more than 1, or returns 0 values
        if (eval_ctx == KEVAL_BOOLEAN) {
            rv = knitx_emit_2(knit, prs, KCALL, nargs); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_2(knit, prs, KCALLR, 1); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
        else {
            rv = knitx_emit_2(knit, prs, KCALL, nargs); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_2(knit, prs, KCALLR, nexpected); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_FUNCTION) {
        int idx = -1;
        rv = kexpr_save_constant(knit, prs, expr, &idx);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
        if (rv != KNIT_OK)
            return rv;
        knit_assert_h(nexpected == 1, "");
        if (eval_ctx == KEVAL_BOOLEAN) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_LITERAL_STR) {
        int idx = -1;
        rv = kexpr_save_constant(knit, prs, expr, &idx);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
        if (rv != KNIT_OK)
            return rv;
        knit_assert_h(nexpected == 1, "");
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
        if (rv != KNIT_OK)
            return rv;
        }
    }
    else if (expr->exptype == KAX_LITERAL_INT) {
        int idx = -1;
        rv = kexpr_save_constant(knit, prs, expr, &idx);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
        if (rv != KNIT_OK)
            return rv;
        }
        else {
            knit_assert_h(nexpected == 1, "");
        }
    }
    else if (expr->exptype == KAX_UN_OP) {
        rv = knitx_emit_expr_eval(knit, prs, expr->u.un.operand, KEVAL_VALUE, 1);
        if (rv != KNIT_OK)
            return rv;
        int insn_op = 0;
        switch (expr->u.un.op) {
            case KAT_SUB: insn_op = KNEG; break;
            case KAT_ADD: insn_op = KNOP; break;
            case KAT_OPU_NOT: insn_op = KNOT; break;
            default: return knit_parse_error(prs, "unexpected unary operand"); break;
        }
        if (insn_op != KNOP) {
            rv = knitx_emit_1(knit, prs, insn_op);  
            if (rv != KNIT_OK)
                return rv;
        }
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_BIN_OP) {
        rv = knitx_emit_expr_eval(knit, prs, expr->u.bin.lhs, KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_expr_eval(knit, prs, expr->u.bin.rhs, KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_1(knit, prs, expr->u.bin.op);  
        if (rv != KNIT_OK)
            return rv;
        if (knit_is_test_op(expr->u.bin.op)) {
            if (eval_ctx == KEVAL_VALUE && nexpected != KRES_UNKNOWN_DISCARD_RET) {
                rv = knitx_emit_1(knit, prs, KSAVETEST);  //save knit->ex.last_cond as a stack value
                if (rv != KNIT_OK)
                    return rv;
                
            }
        }
        else {
            if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
                rv = knitx_emit_1(knit, prs, KTEST);  //test and pop the pushed stack value, saving its truth value to knit->ex.last_cond 
                if (rv != KNIT_OK)
                    return rv;
            }
        }
        //otherwise we don't need to push its result on the stack
    }
    else if (expr->exptype == KAX_LITERAL_LIST) {
        if (nexpected != 1) {
            return knit_parse_error(prs, "expr evaluation of a list cant be discarded, it must return a single value");
        }
        struct knit_expr_darray *elist = &expr->u.elist;
        for (int i=0; i<elist->len; i++) {
            struct knit_expr *expr = elist->data[i];
            knitx_emit_expr_eval(knit, prs, expr, KEVAL_VALUE, 1);
        }
        //now we expect there are n expressions pushed on the stack
        //we emit the instruction KNLIST which pops these and pushes a list containing them
        rv = knitx_emit_2(knit, prs, KNLIST, elist->len); 
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_BOOLEAN) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_LITERAL_DICT) {
        if (nexpected != 1) {
            return knit_parse_error(prs, "expr evaluation of a dict cant be discarded, it must return a single value");
        }
        //we emit the instruction KNDICT which pushes a new empty dict on the stack
        rv = knitx_emit_1(knit, prs, KNDICT); 
        if (rv != KNIT_OK)
            return rv;

        struct knit_expr_darray *elist = &expr->u.elist;
        for (int i=0; i<elist->len; i++) {
            struct knit_expr *expr = elist->data[i];
            if (expr->exptype != KAX_PAIR) {
                return knit_parse_error(prs, "expected an expression of type KAX_PAIR during literal dict evaluation");
            }
            //its evaluation pushes key : value into s[t-3], which is assumed to be a dict
            knitx_emit_2(knit, prs, KPUSH, -1); //duplicate because it's going to be popped by KINDX_SET
            knitx_emit_expr_eval(knit, prs, expr->u.bin.lhs, KEVAL_VALUE, 1); 
            if (rv != KNIT_OK)
                return rv;
            knitx_emit_expr_eval(knit, prs, expr->u.bin.rhs, KEVAL_VALUE, 1); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_1(knit, prs, KINDX_SET);  
            if (rv != KNIT_OK)
                return rv;
        }

        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_INDEX) {
        knitx_emit_expr_eval(knit, prs, expr->u.index.indexed, KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        knitx_emit_expr_eval(knit, prs, expr->u.index.index,   KEVAL_VALUE, 1);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_1(knit, prs, KINDX);  
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_LIST_SLICE) {
        return knit_parse_error(prs, "expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_G) {
        return knit_parse_error(prs, "g eval currently not implemented");
    }
    else if (expr->exptype == KAX_VAR_REF) {
        if (nexpected != 1 && (nexpected != 0 && eval_ctx == KEVAL_BOOLEAN) && nexpected != KRES_UNKNOWN_KEEP_RET) {
            return knit_parse_error(prs, "expr eval of a variable cant be discarded, it must return a single value");
        }

        int vn_idx = expr->u.varref.varname_idx;

        knit_assert_s(vn_idx >= 0 && vn_idx < prs->curblk->locals.len,  "");
        struct knit_varname *vn = knit_get_varname_by_idx(prs->curblk, vn_idx);
        knit_assert_s(vn->name.len > 0,  "");

        if (vn->location == KLOC_UNKNOWN) {
            rv = knitx_varname_set_location(knit, prs->curblk, vn_idx, KLOC_GLOBAL_R); //implicit global
            if (rv != KNIT_OK)
                return rv; 
        }

        if (vn->location == KLOC_GLOBAL_R || vn->location == KLOC_GLOBAL_RW) {
            int idx = -1;
            //save name of global variable in function constants
            rv = knitx_current_block_add_strl_constant(knit, prs, vn->name.str, vn->name.len, &idx);  
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_1(knit, prs, K_GLB_LOAD);  
            if (rv != KNIT_OK)
                return rv;
        }
        else if (vn->location == KLOC_LOCAL_VAR || vn->location == KLOC_ARG) {
            int offset = 0;
            if (vn->location == KLOC_LOCAL_VAR) {
                offset = vn->idx;
            }
            else if (vn->location == KLOC_ARG) {
                offset = -vn->idx - 2; // see [Stack Layout], we assume the current called function is pushed last
            }
            else {
                return knit_parse_error(prs, "variable '%s' unexpected location", vn->name.str); 
            }
            rv = knitx_emit_2(knit, prs, KLLOAD, offset); 
            if (rv != KNIT_OK)
                return rv;
        }
        else {
            knit_assert_s(0,  "unreachable");
        }
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_OBJ_DOT) {
        knit_assert_h(!!expr->u.prefix.chain, "invalid KAX_OBJ_DOT object");
        struct knit_expr *parent = expr->u.prefix.parent;
        struct knit_varname_chain *chain = expr->u.prefix.chain;
        knit_assert_h(parent && chain && chain->name, "invalid KAX_OBJ_DOT object");
        if (parent->exptype == KAX_G) {
            if (chain->next) {
                return knit_parse_error(prs, "obj.obj style access not implemented");
            }
            //hardcoded case for global variables globals are accessed by: g.VARNAME
            int idx = -1;
            knit_assert_h(!!chain->name && chain->name->len > 0, "expected valid var ref str");
            //save name of global variable in function constants
            rv = knitx_current_block_add_strl_constant(knit, prs, chain->name->str, chain->name->len, &idx);  
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_emit_1(knit, prs, K_GLB_LOAD);  
            if (rv != KNIT_OK)
                return rv;
        }
        else {
            rv = knitx_emit_expr_eval(knit, prs, parent, KEVAL_VALUE, 1); 
            if (rv != KNIT_OK)
                return rv;
            while (chain) {
                //this should improved to be statically computed when possible somehow
                int idx = -1;

                struct knit_str *namecpy = NULL;
                rv = knitx_str_new_copy_gcobj(knit, &namecpy, chain->name);
                rv = knitx_block_add_constant(knit, &prs->curblk->block, (struct knit_obj *) namecpy, &idx); 
                if (rv != KNIT_OK)
                    return rv;

                if (!chain->next && eval_ctx == KEVAL_MCALL) {
                    knit_assert_h(nexpected == 2, "MCALL evaluation returns two values");
                    //a.b.c.d
                    //    ^ keep this in order to use it as self in a method call
                    knitx_emit_2(knit, prs, KPUSH, -1);
                }

                rv = knitx_emit_2(knit, prs, KCLOAD, idx);  
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_emit_1(knit, prs, KDOT);  
                if (rv != KNIT_OK)
                    return rv;
                chain = chain->next;
            }
        }
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (eval_ctx == KEVAL_MCALL) {
            if (nexpected != 2)
                return knit_parse_error(prs, "MCALL evaluation must return 2 values");
        }
        else if (nexpected != 1) {
            return knit_parse_error(prs, "expr eval of a variable cant be discarded, it must return a single value");
        }
    }
    else if (expr->exptype == KAX_LITERAL_NULL  ||
             expr->exptype == KAX_LITERAL_TRUE  ||
             expr->exptype == KAX_LITERAL_FALSE   ) 
    {
        int emkind = -1;
        if (nexpected != 1) {
            return knit_parse_error(prs, "expr eval of a literal cant be discarded, it must return a single value");
        }
        switch (expr->exptype) {
            case KAX_LITERAL_TRUE:  emkind = KEMTRUE;  break;
            case KAX_LITERAL_FALSE: emkind = KEMFALSE; break;
            case KAX_LITERAL_NULL:  emkind = KEMNULL;  break;
            default: return knit_parse_error(prs, "unexpected experssion type");
        }
        rv = knitx_emit_2(knit, prs, KEMIT, emkind); 
        if (rv != KNIT_OK)
            return rv;
        if (eval_ctx == KEVAL_BOOLEAN || nexpected == KRES_UNKNOWN_DISCARD_RET) {
            //this can be optimized to const fold this during compilation
            rv = knitx_emit_1(knit, prs, KTEST); 
            if (rv != KNIT_OK)
                return rv;
        }
    }
    else if (expr->exptype == KAX_LOGICAL_BINOP) {
        if (eval_ctx != KEVAL_BOOLEAN &&  nexpected != 1) {
            return knit_parse_error(prs, "expr eval of a literal cant be discarded, it must return a single value");
        }
        int ectx = eval_ctx;
        if (nexpected == KRES_UNKNOWN_DISCARD_RET)
            ectx = KEVAL_BOOLEAN;
        rv = knitx_emit_logical_operation(knit, prs, expr, ectx); 
        if (rv != KNIT_OK)
            return rv;
    }
    else {
        return knit_parse_error(prs, "unexpected experssion type");
    }
    return KNIT_OK;
}

//turn a binary op into a sequence of bytecode insns that result in its result being at the top of the stack
static int kexpr_binoperation(struct knit *knit, struct knit_prs *prs, int op_token, struct knit_expr *lhs_expr, struct knit_expr *rhs_expr) {
    struct knit_expr *prs_expr = &prs->curblk->expr;
    if (knit_is_logical_op_token(op_token)) {
        prs_expr->exptype = KAX_LOGICAL_BINOP;
        prs_expr->u.logic_bin.op = op_token; //yeah, technically storing the token type, this should probably done for the rest of them
        prs_expr->u.logic_bin.lhs = lhs_expr;
        prs_expr->u.logic_bin.rhs = rhs_expr;
        prs_expr->u.logic_bin.plist = NULL;
    }
    else {
        int insn_type = 0;
        switch (op_token) {
            case KAT_ADD:     insn_type = KADD;       break;
            case KAT_SUB:     insn_type = KSUB;       break;
            case KAT_MUL:     insn_type = KMUL;       break;
            case KAT_DIV:     insn_type = KDIV;       break;
            case KAT_MOD:     insn_type = KMOD;       break;
            case KAT_OP_EQ:   insn_type = KTESTEQ;    break;
            case KAT_OP_NEQ:  insn_type = KTESTNEQ;   break;
            case KAT_OP_GT:   insn_type = KTESTGT;    break;
            case KAT_OP_LT:   insn_type = KTESTLT;    break;
            case KAT_OP_GTEQ: insn_type = KTESTGTEQ;  break;
            case KAT_OP_LTEQ: insn_type = KTESTLTEQ;  break;
            default: {
                knit_assert_h(0, "invalid op type");
            } break;
        }
        
        prs_expr->exptype = KAX_BIN_OP;
        prs_expr->u.bin.op = insn_type;
        prs_expr->u.bin.lhs = lhs_expr;
        prs_expr->u.bin.rhs = rhs_expr;
    }
    return KNIT_OK;
}

static int kexpr_expr(struct knit *knit, struct knit_prs *prs, int min_prec) {
    //https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing
    int rv = kexpr_atom(knit, prs);   
    if (rv != KNIT_OK)
        return rv;

    struct knit_tok *curtok = knitx_lexer_curtok(knit, prs);
    if (!curtok) {
        return knit_error(knit, KNIT_SYNTAX_ERR, "expecting an expression");
    }
    struct knit_expr *lhs_expr = NULL; //we accumulate into this
    while (curtok && knit_tok_is(curtok->toktype, KAT_IS_BINOP) && (knit_tok_prec(curtok->toktype) >= min_prec)) {
        rv = knitx_save_expr(knit, prs, &lhs_expr);  
        if (rv != KNIT_OK)
            return rv;
        int op = curtok->toktype;
        int prec = knit_tok_prec(curtok->toktype);
        int assoc = knit_tok_is(curtok->toktype, KAT_IS_LEFT_ASSOC) ? KAT_IS_LEFT_ASSOC : KAT_IS_RIGHT_ASSOC;
        int next_min_prec = (assoc == KAT_IS_LEFT_ASSOC) ? prec + 1 : prec;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; 
        struct knit_expr *rhs_expr = NULL;
        rv = kexpr_expr(knit, prs, next_min_prec);  
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_save_expr(knit, prs, &rhs_expr);  
        if (rv != KNIT_OK)
            return rv;
        rv = kexpr_binoperation(knit, prs, op, lhs_expr, rhs_expr);  
        if (rv != KNIT_OK)
            return rv;
        curtok = knitx_lexer_curtok(knit, prs);
    }
    if (!curtok) {
        knit_clear_error(knit);
    }
    //at return, the resulting experssion will be at .prs.expr
    return rv;
}

static int knitx_expr(struct knit *knit, struct knit_prs *prs) {
    return kexpr_expr(knit, prs, 1);
}

//boolean return value
static int knitx_is_lvalue(struct knit *knit, struct knit_expr *exp) {
    //TODO: there are other kinds of lvalues
    return exp->exptype == KAX_VAR_REF || exp->exptype == KAX_OBJ_DOT || exp->exptype == KAX_INDEX;
}

static int knitx_curtok_in_first_of_expr(struct knit *knit, struct knit_prs *prs) {
    return K_TOKEN_MATCHES(KAT_INTLITERAL) ||
           K_TOKEN_MATCHES(KAT_STRLITERAL) ||
           K_TOKEN_MATCHES(KAT_VAR)        ||
           K_TOKEN_MATCHES(KAT_OBRACKET)   ||
           K_TOKEN_MATCHES(KAT_FUNCTION)   ||
           K_TOKEN_MATCHES(KAT_TRUE)       ||
           K_TOKEN_MATCHES(KAT_FALSE)      ||
           K_TOKEN_MATCHES(KAT_NULL)       ||
           K_TOKEN_MATCHES(KAT_OPAREN)     ||
           K_TOKEN_MATCHES(KAT_OPU_NOT)    ||
           K_TOKEN_MATCHES(KAT_ADD)    ||
           K_TOKEN_MATCHES(KAT_SUB);
    /*KAT_ADD '+' and KAT_SUB '-' can either be unary or binary*/
}

//assumes stmt_array_out is pointing to an uninitalized stmt_darray
static int knit_prs_sblock_into_darray(struct knit *knit, struct knit_prs *prs, struct knit_stmt_darray *stmt_array_out)
{
    if (!K_TOKEN_MATCHES(KAT_OCURLY)) {
        return knit_error_expected(knit, prs, "{", "");
    }
    int rv = KNIT_OK;
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'{'

    knit_stmt_darray_init(stmt_array_out, 1);
    if (rv != KNIT_STMT_DARRAY_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize stmt dynamic array"); 
    }
    while (!K_TOKEN_MATCHES(KAT_EOF) && !K_TOKEN_MATCHES(KAT_CCURLY)) {
        struct knit_stmt *stmt = NULL;
        rv = knitx_stmt_prs_new(knit, prs, KSTMT_ALL, &stmt); 
        if (rv != KNIT_OK)
            return rv;
        rv = knit_stmt_darray_push(stmt_array_out, &stmt);
        if (rv != KNIT_STMT_DARRAY_OK) {
            return knit_error(knit, KNIT_RUNTIME_ERR, "failed to push stmt into dynamic array"); 
        }
    }
    if (!K_TOKEN_MATCHES(KAT_CCURLY)) {
        return knit_error_expected(knit, prs, "}", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'}'
    return KNIT_OK;
}

static int knit_prs_sblock_stmt_new(struct knit *knit, struct knit_prs *prs, struct knit_stmt **stmt_out) {
    void *p = NULL;
    int rv  = knitx_tmalloc(knit, sizeof(struct knit_stmt), &p);  
    if (rv != KNIT_OK)
        return rv;
    struct knit_stmt *sblock_stmt = p;
    struct knit_stmt_darray *array = &sblock_stmt->u._sblock.body;
    rv = knit_prs_sblock_into_darray(knit, prs, array); 
    if (rv != KNIT_OK) {
        knitx_tfree(knit, p);
        return knit_error(knit, KNIT_RUNTIME_ERR, "failed to allocate mmeory for sblock stmt"); 
    }
    sblock_stmt->stmttype = KSTMT_SBLOCK;
    *stmt_out = sblock_stmt;
    return KNIT_OK;
}

static int knitx_prs_if_stmt(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt_out) {
    int rv = KNIT_OK;
    knit_assert_h(K_TOKEN_MATCHES(KAT_IF), "");
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'if'
    if (!knitx_curtok_in_first_of_expr(knit, prs)) {
        return knit_parse_error(prs, "expected a boolean expression");
    }
    struct knit_expr *condition = NULL;
    rv = knitx_expr(knit, prs); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_save_expr(knit, prs, &condition);  
    if (rv != KNIT_OK)
        return rv;
    //?knitx_expr_destroy(knit, prs, root_expr);

    struct knit_stmt_darray *stmt_array = &stmt_out->u._if.body;
    rv = knit_prs_sblock_into_darray(knit, prs, stmt_array); 
    if (rv != KNIT_OK)
        return rv;

    if (K_TOKEN_MATCHES(KAT_ELSE)) {
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'else'

        if (K_TOKEN_MATCHES(KAT_IF)) {
            //recursive
            struct knit_stmt *else_if_stmt = NULL;
            rv = knitx_prs_if_stmt_new(knit, prs, &else_if_stmt); 
            if (rv != KNIT_OK)
                return rv; //ml
            stmt_out->u._if._else = else_if_stmt;
        }
        else if (K_TOKEN_MATCHES(KAT_OCURLY)) {
            struct knit_stmt *sblock_stmt = NULL;
            rv = knit_prs_sblock_stmt_new(knit, prs, &sblock_stmt); 
            if (rv != KNIT_OK)
                return rv;
            stmt_out->u._if._else = sblock_stmt;
        }
        else {
            return knit_error_expected(knit, prs, "else if or else {", "");
        }
    }
    else {
        stmt_out->u._if._else = NULL;
    }
    stmt_out->stmttype = KSTMT_IF;
    stmt_out->u._if.cond = condition;
    return KNIT_OK;
}

//parse and box into a dynamically allocated statement
static int knitx_prs_if_stmt_new(struct knit *knit, struct knit_prs *prs, struct knit_stmt **if_stmt_out) {
    void *p = NULL;

    int rv  = knitx_tmalloc(knit, sizeof(struct knit_stmt), &p);  
    if (rv != KNIT_OK)
        return rv;
    struct knit_stmt *if_stmt = p;
    rv = knitx_prs_if_stmt(knit, prs, if_stmt);
    if (rv != KNIT_OK) {
        knitx_tfree(knit, if_stmt);
        return rv;
    }
    *if_stmt_out = if_stmt;
    return KNIT_OK;
}

static int knitx_prs_for_stmt(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt_out) {
    int rv = KNIT_OK;
    knit_assert_h(K_TOKEN_MATCHES(KAT_FOR), "");
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'for'
    struct knit_stmt *init_stmt = NULL;
    struct knit_expr *cond_expr = NULL;
    struct knit_stmt *inc_stmt = NULL;
    int has_paren = 0;
    if (K_TOKEN_MATCHES(KAT_OPAREN)) {
        has_paren = 1;
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; // (
    }
    //init
    if (!K_TOKEN_MATCHES(KAT_SEMICOLON)) {
        //TODO defer emitting code for this statement when it becomes possible in the future
        //(we can store statements to an intermediate struct, then execute that later, currently we can do that with exprs but not statements)
        rv = knitx_stmt_prs_new(knit, prs, KSTMT_EXPR | KSTMT_ASSIGN | KLEAVE_SEMICOLON, &init_stmt); 
        if (rv != KNIT_OK)
            return rv;
    }
    if (!K_TOKEN_MATCHES(KAT_SEMICOLON)) {
        return knit_error_expected(knit, prs, ";", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; // ;

    //cond
    if (!K_TOKEN_MATCHES(KAT_SEMICOLON)) {
        rv = knitx_expr(knit, prs); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_save_expr(knit, prs, &cond_expr); 
        if (rv != KNIT_OK)
            return rv;
    }
    if (!K_TOKEN_MATCHES(KAT_SEMICOLON)) {
        return knit_error_expected(knit, prs, ";", "");
    }
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; // ;

    //increment
    if (!(!has_paren && !K_TOKEN_MATCHES(KAT_OCURLY)) || (has_paren && !K_TOKEN_MATCHES(KAT_CPAREN))) {
        rv = knitx_stmt_prs_new(knit, prs, KSTMT_ASSIGN | KSTMT_EXPR | KLEAVE_SEMICOLON, &inc_stmt); 
        if (rv != KNIT_OK)
            return rv;
    }

    if (has_paren) {
        if (!K_TOKEN_MATCHES(KAT_CPAREN)) {
            return knit_error_expected(knit, prs, ")", "");
        }
        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; // )
    }

    struct knit_stmt_darray *stmt_array = &stmt_out->u._for.body;
    rv = knit_prs_sblock_into_darray(knit, prs, stmt_array); 
    if (rv != KNIT_OK)
        return rv;

    stmt_out->stmttype = KSTMT_FOR;
    stmt_out->u._for.init   = init_stmt;
    stmt_out->u._for.mutate = inc_stmt;
    stmt_out->u._for.cond   = cond_expr;

    return KNIT_OK;
}

static int knitx_while_stmt(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt_out) {
    int rv = KNIT_OK;
    knit_assert_h(K_TOKEN_MATCHES(KAT_WHILE), "");
    if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //'while'
    if (!knitx_curtok_in_first_of_expr(knit, prs)) {
        return knit_parse_error(prs, "expected a boolean expression");
    }
    struct knit_expr *condition = NULL;
    rv = knitx_expr(knit, prs); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_save_expr(knit, prs, &condition);  
    if (rv != KNIT_OK)
        return rv;

    struct knit_stmt_darray *stmt_array = &stmt_out->u._while.body;
    rv = knit_prs_sblock_into_darray(knit, prs, stmt_array); 
    if (rv != KNIT_OK)
        return rv;

    stmt_out->u._while.cond = condition;
    stmt_out->stmttype = KSTMT_WHILE;
    return KNIT_OK;
}

//parse a statement and save it in *stmt_out
static int knitx_stmt_prs(struct knit *knit, struct knit_prs *prs, int allowed_stmts, struct knit_stmt *stmt_out) {
    int a = allowed_stmts;
    int skip_semicolon = !(a & KLEAVE_SEMICOLON);
    int rv = KNIT_OK;
    rv = knitx_skip_nl(knit, prs);
    if (rv != KNIT_OK)
        return rv;
    if (((a & KSTMT_ASSIGN) || (a & KSTMT_EXPR)) && knitx_curtok_in_first_of_expr(knit, prs)) {
        rv = knitx_expr(knit, prs); 
        if (rv != KNIT_OK)
            return rv;
        if ((a & KSTMT_ASSIGN) && K_TOKEN_MATCHES(KAT_ASSIGN)) {
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; // skip '='
            if (!knitx_is_lvalue(knit, &prs->curblk->expr)) {
                return knit_error(knit, KNIT_SYNTAX_ERR, "attempting to assign to a non-lvalue");
            }
            struct knit_expr *lhs_expr = NULL;
            int rv = knitx_save_expr(knit, prs, &lhs_expr);  
            if (rv != KNIT_OK)
                return rv;

            rv = knitx_expr(knit, prs);  
            if (rv != KNIT_OK)
                return rv;
            struct knit_expr *rhs_expr = NULL;
            rv = knitx_save_expr(knit, prs, &rhs_expr);  
            if (rv != KNIT_OK)
                return rv;

            //?knitx_expr_destroy(knit, prs, root_expr);
            stmt_out->stmttype = KSTMT_ASSIGN;
            stmt_out->u._assign.lhs = lhs_expr;
            stmt_out->u._assign.rhs = rhs_expr;
        }
        else if (a & KSTMT_EXPR) {
            struct knit_expr *root_expr = NULL;
            rv = knitx_save_expr(knit, prs, &root_expr);  
            if (rv != KNIT_OK)
                return rv;
            //?knitx_expr_destroy(knit, prs, root_expr);
            stmt_out->stmttype = KSTMT_EXPR;
            stmt_out->u._expr = root_expr;
        }
        else {
            return knit_error_expected(knit, prs, "a statement", "");
        }
        if (skip_semicolon && K_TOKEN_MATCHES(KAT_SEMICOLON)) {
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //skip ';'
        }
    }
    else if ((a & KSTMT_IF) && K_TOKEN_MATCHES(KAT_IF)) {
        rv = knitx_prs_if_stmt(knit, prs, stmt_out); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if ((a & KSTMT_FOR) && K_TOKEN_MATCHES(KAT_FOR)) {
        rv = knitx_prs_for_stmt(knit, prs, stmt_out); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if ((a & KSTMT_WHILE) && K_TOKEN_MATCHES(KAT_WHILE)) {
        rv = knitx_while_stmt(knit, prs, stmt_out); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if ((a & KSTMT_RETURN) && K_TOKEN_MATCHES(KAT_RETURN)) {

        if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //return
        rv = knitx_expr(knit, prs); 
        if (rv != KNIT_OK)
            return rv;
        struct knit_expr *root_expr = NULL;
        rv = knitx_save_expr(knit, prs, &root_expr);  
        if (rv != KNIT_OK)
            return rv;
        //?knitx_expr_destroy(knit, prs, root_expr);
        stmt_out->u._expr = root_expr;
        stmt_out->stmttype = KSTMT_RETURN;
        if (skip_semicolon && K_TOKEN_MATCHES(KAT_SEMICOLON)) {
            if ((rv = knitx_lexer_skip(knit, &prs->lex)) != KNIT_OK) return rv; //skip ';'
        }
    }
    else {
        return knit_error_expected(knit, prs, "a statement", "");
    }
    rv = knitx_skip_nl(knit, prs);
    return rv;

}

static int knitx_emit_ret(struct knit *knit, struct knit_prs *prs, int count) {
    if (count > 1 || count < 0)
        return knit_parse_error(prs, "returning multiple values is not implemented");
    return knitx_emit_2(knit, prs, KRET, count);
}

static int knitx_stmt_emit(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt) {
    int rv = KNIT_OK;
    if (stmt->stmttype == KSTMT_EXPR) {
        rv = knitx_emit_expr_eval(knit, prs,  stmt->u._expr, KEVAL_VALUE, KRES_UNKNOWN_DISCARD_RET); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (stmt->stmttype == KSTMT_ASSIGN) {
        rv = knitx_emit_assignment(knit, prs, stmt->u._assign.lhs, stmt->u._assign.rhs); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (stmt->stmttype == KSTMT_FOR) {

        //for stmt
        //emit init first THEN
        rv = knitx_stmt_emit(knit, prs, stmt->u._for.init); 
        if (rv != KNIT_OK)
            return rv;
        
        int L1_address = prs->curblk->block.insns.len; //next instruction's address
        rv = knitx_emit_expr_eval(knit, prs, stmt->u._for.cond, KEVAL_BOOLEAN, 0);  
        if (rv != KNIT_OK)
            return rv;

        knitx_emit_2(knit, prs, KJMPFALSE, KINSN_ADDR_UNK); //will be backpatched to point to L4
        if (rv != KNIT_OK)
            return rv; 
        struct knit_patch_list *L4_pos = NULL; //L2
        //reference the KJMPFALSE instruction which will be backpatched
        rv = knit_patch_loc_new_or_insert(knit, prs->curblk->block.insns.len - 1, &L4_pos); 
        if (rv != KNIT_OK)
            return rv;

        rv = knitx_stmt_array_emit(knit, prs, &stmt->u._for.body); 
        if (rv != KNIT_OK)
            return rv;

        rv = knitx_stmt_emit(knit, prs, stmt->u._for.mutate); 
        if (rv != KNIT_OK)
            return rv;
        knitx_emit_2(knit, prs, KJMP, L1_address); //jmp to L1 to check condition again

        int L4_address = prs->curblk->block.insns.len; //next instruction's address
        rv = knit_patch_loc_list_patch_and_destroy(knit, &prs->curblk->block, &L4_pos, L4_address);  //Patch L4
        if (rv != KNIT_OK)
            return rv; 

        /*
        *
        * L0:
        * INIT_STATEMENTS
        * L1:
        * COND_STATEMENTS
        * L2:
        * BODY_STATEMENTS
        * L3:
        * INCR_STATEMENTS
        * L4:
        *
        * psuedo code
        *
        * init
        * L1
        * if (!cond) 
        *      goto L4;
        * body statements;
        * incr;
        * goto L1;
        * L4:
        *
        */

    }
    else if (stmt->stmttype == KSTMT_WHILE) {
        //while stmt
            /*
            * L1: while (C) { [false? jmp L2, True? exec block then jmp L1]
            *  S1, S2 ...
            *  jmp L3:
            * }
            * L2:
            */

        int L1_address = prs->curblk->block.insns.len; //next instruction's address
        rv = knitx_emit_expr_eval(knit, prs, stmt->u._while.cond, KEVAL_BOOLEAN, 0);  
        if (rv != KNIT_OK)
            return rv;
        //?knitx_expr_destroy(knit, prs, condition);
        knitx_emit_2(knit, prs, KJMPFALSE, KINSN_ADDR_UNK); // will be backpatched to point to L2
        struct knit_patch_list *L2_pos = NULL; //L2
        rv = knit_patch_loc_new_or_insert(knit, prs->curblk->block.insns.len - 1, &L2_pos); 
        if (rv != KNIT_OK)
            return rv;

        rv = knitx_stmt_array_emit(knit, prs, &stmt->u._while.body); 
        if (rv != KNIT_OK)
            return rv;

        knitx_emit_2(knit, prs, KJMP, L1_address); //jmp to L1 to check condition again

        int L2_address = prs->curblk->block.insns.len; //next instruction's address
        rv = knit_patch_loc_list_patch_and_destroy(knit, &prs->curblk->block, &L2_pos, L2_address); 
        if (rv != KNIT_OK)
            return rv; //Patch L2
    }
    else if (stmt->stmttype == KSTMT_RETURN) {
        rv = knitx_emit_expr_eval(knit, prs,  stmt->u._expr, KEVAL_VALUE, 1); 
        if (rv != KNIT_OK)
            return rv;
        rv = knitx_emit_ret(knit, prs, 1); 
        if (rv != KNIT_OK)
            return rv;
    }
    else if (stmt->stmttype == KSTMT_IF) {

        /*
        * L1: if (C) { [false? jmp L2, True? exec block then jmp L3]
        *  S1, S2 ...
        *  jmp L3:
        * }
        * L2: else {
        * }
        * L3:
        *
        */

        rv = knitx_emit_expr_eval(knit, prs, stmt->u._if.cond, KEVAL_BOOLEAN, 0);  
        if (rv != KNIT_OK)
            return rv;

        struct knit_patch_list *L2_pos = NULL;
        knitx_emit_2(knit, prs, KJMPFALSE, KINSN_ADDR_UNK); //jump to the else part, or the statement after if in case there wasnt an else
        rv = knit_patch_loc_new_or_insert(knit, prs->curblk->block.insns.len - 1, &L2_pos); 
        if (rv != KNIT_OK)
            return rv;

        rv = knitx_stmt_array_emit(knit, prs, &stmt->u._if.body); 
        if (rv != KNIT_OK)
            return rv;

        struct knit_patch_list *L3_pos = NULL; 
        if (stmt->u._if._else != NULL) {
            //we jmp unconditionally in the if { block .. JMP }, this jmp is only needed if there is an else
            knitx_emit_2(knit, prs, KJMP, KINSN_ADDR_UNK); //jump the else body in case the initial if was matched
            rv = knit_patch_loc_new_or_insert(knit, prs->curblk->block.insns.len - 1, &L3_pos); 
            if (rv != KNIT_OK)
                return rv;
        }

        //patch JMPFALSE to this point to skip if body
        int L2_address = prs->curblk->block.insns.len; 
        rv = knit_patch_loc_list_patch_and_destroy(knit, &prs->curblk->block, &L2_pos, L2_address); 
        if (rv != KNIT_OK)
            return rv;
        L2_pos = NULL;

        if (stmt->u._if._else != NULL) {
            rv = knitx_stmt_emit(knit, prs, stmt->u._if._else); 
            if (rv != KNIT_OK)
                return rv;
            int L3_address = prs->curblk->block.insns.len; 
            rv = knit_patch_loc_list_patch_and_destroy(knit, &prs->curblk->block, &L3_pos, L3_address); //patch previous unconditional jmp to this point
            if (rv != KNIT_OK)
                return rv; 
        }
    }
    else if (stmt->stmttype == KSTMT_SBLOCK) {
        rv = knitx_stmt_array_emit(knit, prs, &stmt->u._sblock.body); 
        if (rv != KNIT_OK)
            return rv;
    }
    else {
        knit_assert_h(0, "unknown stmt type");
    }
    return rv;
}

static int knitx_stmt_array_emit(struct knit *knit, struct knit_prs *prs, struct knit_stmt_darray *array) {
    for (int i=0; i<array->len; i++) {
        struct knit_stmt *stmt = array->data[i];
        int rv = knitx_stmt_emit(knit, prs, stmt); 
        if (rv != KNIT_OK)
            return rv;
    }
    return KNIT_OK;
}

//parse a statement and save it in a dynamically allocated object
static int knitx_stmt_prs_new(struct knit *knit, struct knit_prs *prs, int allowed_stmts, struct knit_stmt **stmt_out) {
    int rv = KNIT_OK;
    struct knit_stmt *stmt;
    void *p;
    rv  = knitx_tmalloc(knit, sizeof(struct knit_stmt), &p);  
    if (rv != KNIT_OK)
        return rv;
    stmt = p;
    rv = knitx_stmt_prs(knit, prs, allowed_stmts, stmt); 
    *stmt_out = stmt;
    return rv;
}

static int knitx_stmt_destroy(struct knit *knit, struct knit_prs *prs, struct knit_stmt *stmt) {
    knitx_stmt_deinit(knit, prs, stmt);
    knitx_tfree(knit, stmt);
    return KNIT_OK;
}

//parse and emit a statement
static int knitx_stmt_prs_emit(struct knit *knit, struct knit_prs *prs, int allowed_stmts) {
    /*
    symbol          first
    program         Name + '(' 
    stat            Name + '(' 
    var             Name + '('
    prefixexp       Name + '('
    exp             null + false + Numeral + LiteralString + '[' + '(' + Name + f(unop)
    literallist     '['
    */
    int rv = KNIT_OK;
    struct knit_stmt stmt;
    rv = knitx_stmt_prs(knit, prs, allowed_stmts, &stmt); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_stmt_emit(knit, prs, &stmt); 
    if (rv != KNIT_OK)
        return rv;
    knitx_stmt_deinit(knit, prs, &stmt);
    return KNIT_OK;

}

static int knitx_prog(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    while (!K_TOKEN_MATCHES(KAT_EOF) && rv == KNIT_OK) {
        rv = knitx_stmt_prs_emit(knit, prs, KSTMT_ALL); 
    }
    
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_emit_ret(knit, prs, 0); 
    return rv;
}

#undef K_LA_TOKEN_MATCHES
#undef K_TOKEN_MATCHES
/*END OF PARSING FUNCS*/

/*raw indices (not relative to bsp)*/
static int knitx_stack_assign_s(struct knit *knit, struct knit_stack *stack, int dest, int src) {
    stack->vals.data[dest] = stack->vals.data[src];
    return KNIT_OK;
}

static int knitx_stack_assign_o(struct knit *knit, struct knit_stack *stack, int dest, struct knit_obj *obj) {
    stack->vals.data[dest] = obj;
    return KNIT_OK;
}

//assign null functions are only used for testing (they should be disabled for release)
//set a range to null
static int knitx_stack_assign_range_null(struct knit *knit, struct knit_stack *stack, int begin, int end) {
    for (int i=begin; i<end; i++) {
        stack->vals.data[i] = NULL;
    }
    return KNIT_OK;
}

//set a value to null
static int knitx_stack_assign_null(struct knit *knit, struct knit_stack *stack, int idx) {
    stack->vals.data[idx] = NULL;
    return KNIT_OK;
}

/*r means no incref/decrfs*/
static int knitx_stack_rpop(struct knit *knit, struct knit_stack *stack, int count) {
    knit_assert_s(count <= stack->vals.len && count > 0, "");
    knitx_stack_assign_range_null(knit, stack, stack->vals.len - count, stack->vals.len);
    stack->vals.len -= count;
    return KNIT_OK;
}

static int knitx_stack_rpush(struct knit *knit, struct knit_stack *stack, struct knit_obj *obj) {
    stack->vals.data[stack->vals.len] = obj;
    stack->vals.len++;
    return KNIT_OK;
}

static inline int knitx_op_do_binop(struct knit *knit, struct knit_obj *a, struct knit_obj *b, struct knit_obj **r, int op) 
{
    if (a->u.ktype == KNIT_INT && b->u.ktype == KNIT_INT) {
        struct knit_int *ai = (struct knit_int *) a;
        struct knit_int *bi = (struct knit_int *) b;
        if ((op == KDIV || op == KMOD) && (!bi->value)) {
            *r = NULL;
            return knit_error(knit, KNIT_RUNTIME_ERR, "division by zero");
        }
        struct knit_int *ri = NULL;
        int rv = knitx_int_new_gcobj(knit, &ri, 0); 
        if (rv != KNIT_OK)
            return rv;
        *r = ktobj(ri); 
        switch (op) {
            case KADD: ri->value = ai->value + bi->value; break;
            case KSUB: ri->value = ai->value - bi->value; break;
            case KMUL: ri->value = ai->value * bi->value; break;
            case KDIV: ri->value = ai->value / bi->value; break;
            case KMOD: ri->value = ai->value % bi->value; break;
            default:
                return knit_runtime_error(knit, "unsupported op for ints: %s", knit_insn_name(op));
        }
    }
    else if (a->u.ktype == KNIT_STR && b->u.ktype == KNIT_STR) {
        struct knit_str *as = (struct knit_str *) a;
        struct knit_str *bs = (struct knit_str *) b;
        if (op == KADD) {
            struct knit_str *rs = NULL;
            int rv = knitx_str_new_copy_gcobj(knit, &rs, as); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_str_strlappend(knit, rs, bs->str, bs->len); 
            if (rv != KNIT_OK)
                return rv;
            *r = ktobj(rs);
        }
        else {
            return knit_runtime_error(knit, "unsupported op for strings: %s", knit_insn_name(op));
        }
    }
    else {
        return knit_runtime_error(knit, "knitx_op_add(): unsupported types for %s: %s and %s", knit_insn_name(op), knitx_obj_type_name(knit, a), knitx_obj_type_name(knit, b));
    }
    return KNIT_OK;
}

static int knitx_op_exec_binop(struct knit *knit, struct knit_stack *stack, int op) {
    knit_assert_s(stack->vals.len >= 2, "");
    struct knit_obj *a = stack->vals.data[stack->vals.len - 2];
    struct knit_obj *b = stack->vals.data[stack->vals.len - 1];
    struct knit_obj *r = NULL;
    int rv = knitx_op_do_binop(knit, a, b, &r, op);
    if (rv != KNIT_OK)
        return rv;
    knitx_obj_decref(knit, a);
    knitx_obj_decref(knit, b);
    knitx_stack_assign_range_null(knit, stack, stack->vals.len - 1, stack->vals.len);
    knit_assert_h((rv == KNIT_OK && r) || (rv != KNIT_OK && !r), "");
    knitx_stack_assign_o(knit, stack, stack->vals.len - 2, ktobj(r));
    stack->vals.len--; //pop1
    return KNIT_OK;
}

static inline int knitx_op_do_test_binop(struct knit *knit, struct knit_obj *a, struct knit_obj *b, int op) {
    if (a->u.ktype == KNIT_INT && b->u.ktype == KNIT_INT) {
        struct knit_int *ai = (struct knit_int *) a;
        struct knit_int *bi = (struct knit_int *) b;
        switch (op) {
            case KTESTEQ:   knit->ex.last_cond = ai->value == bi->value; break;
            case KTESTNEQ:  knit->ex.last_cond = ai->value != bi->value; break;
            case KTESTGT:   knit->ex.last_cond = ai->value >  bi->value; break;
            case KTESTLT:   knit->ex.last_cond = ai->value <  bi->value; break;
            case KTESTGTEQ: knit->ex.last_cond = ai->value >= bi->value; break;
            case KTESTLTEQ: knit->ex.last_cond = ai->value <= bi->value; break;
            default:
                return knit_runtime_error(knit, "unsupported op for ints: %s", knit_insn_name(op));
        }
    }
    else if (a->u.ktype == KNIT_STR && b->u.ktype == KNIT_STR) {
        struct knit_str *as = (struct knit_str *) a;
        struct knit_str *bs = (struct knit_str *) b;
        if (op == KTESTEQ) {
            knit->ex.last_cond = knitx_str_streq(knit, as, bs);
        }
        else {
            return knit_runtime_error(knit, "unsupported op for strings: %s", knit_insn_name(op));
        }
    }
    else {
        return knit_runtime_error(knit, "knitx_op_do_test_binop(): unsupported types for %s: %s and %s", knit_insn_name(op), knitx_obj_type_name(knit, a), knitx_obj_type_name(knit, b));
    }
    return KNIT_OK;
}

static int knitx_op_exec_test_binop(struct knit *knit, struct knit_stack *stack, int op) {
    knit_assert_s(stack->vals.len >= 2, "");
    struct knit_obj *a = stack->vals.data[stack->vals.len - 2];
    struct knit_obj *b = stack->vals.data[stack->vals.len - 1];
    int rv = knitx_op_do_test_binop(knit, a, b, op);
    if (rv != KNIT_OK)
        return rv;
    knitx_obj_decref(knit, a);
    knitx_obj_decref(knit, b);
    knitx_stack_assign_range_null(knit, stack, stack->vals.len - 1, stack->vals.len);
    stack->vals.len -= 2; //pop2
    return rv;
}

//pushes it to stack
static int knitx_do_global_load(struct knit *knit, struct knit_str *name) {
    struct knit_exec_state *exs = &knit->ex;
    struct knit_vars_hasht_iter iter;
    int rv = knit_vars_hasht_find(&exs->global_ht, name, &iter);
    if (rv != KNIT_VARS_HASHT_OK) {
        if (rv == KNIT_VARS_HASHT_NOT_FOUND)
            return knit_error(knit, KNIT_NOT_FOUND, "variable '%s' is undefined", name->str);
        else 
            return knit_error(knit, KNIT_RUNTIME_ERR, "an error occured while trying to lookup a variable in knit_vars_hasht_find()");
    }
    rv = knitx_stack_rpush(knit, &exs->stack, iter.pair->value);
    if (rv != KNIT_OK)
        return knit_error(knit, KNIT_RUNTIME_ERR, "an error occured while trying to push a value to the stack");
    return KNIT_OK;
}

//doesn't own name
static int knitx_do_global_assign(struct knit *knit, struct knit_str *name, struct knit_obj *rhs) {
    struct knit_exec_state *exs = &knit->ex;
    struct knit_vars_hasht_iter iter;
    int rv = knit_vars_hasht_find(&exs->global_ht, name, &iter);
    if (rv == KNIT_VARS_HASHT_OK) {
        //todo destroy previous value 
        iter.pair->value = rhs;
    }
    else if (rv == KNIT_VARS_HASHT_NOT_FOUND) {
        struct knit_str *new_str;
        rv = knitx_str_new_strcpy_gcobj(knit, &new_str, name->str); 
        if (rv != KNIT_OK)
            return rv;
        rv = knit_vars_hasht_insert(&exs->global_ht, name, &rhs);
    }

    if (rv != KNIT_VARS_HASHT_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_do_global_assign(): assignment failed");
    }
    return KNIT_OK;
}

static int knitx_eval_index_expr(struct knit *knit, struct knit_prs *prs, struct knit_expr *indexed, struct knit_expr *index) {
    int rv = KNIT_OK;
    //we can optimize here
    rv = knitx_emit_expr_eval(knit, prs,  indexed, KEVAL_VALUE, 1); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_emit_expr_eval(knit, prs,  index, KEVAL_VALUE, 1); 
    if (rv != KNIT_OK)
        return rv;
    return KNIT_OK;
}

//useless function used as a debugging breakpoint
static inline void kstepi() { return; }

static int knitx_exec(struct knit *knit) {
    struct knit_stack *stack = &knit->ex.stack;
    struct knit_frame_darray *frames = &knit->ex.stack.frames;
    struct knit_objp_darray *stack_vals = &knit->ex.stack.vals;

    knit_assert_h(frames->len > 0, "");
    struct knit_frame *top_frm = &frames->data[frames->len-1];
    struct knit_block *block = top_frm->u.kf.block;
    knit_assert_h(top_frm->bsp >= 0 && top_frm->bsp <= stack_vals->len, "");

    int rv = KNIT_OK;
    while (1) {
        knit_assert_s(top_frm->u.kf.ip < block->insns.len, "executing out of range instruction");
        struct knit_insn *insn = &block->insns.data[top_frm->u.kf.ip];
        int t = stack_vals->len; //values stack size, top of stack is stack_vals->data[t-1]
        int op = insn->insn_type;
        rv = KNIT_OK;
        if (op == KPUSH) { 
            int offset = insn->op1 < 0 ? insn->op1 + stack_vals->len : insn->op1;
            knit_assert_s(offset >= 0 && offset < stack_vals->len, "loading out of range stack value");
            rv = knitx_stack_rpush(knit, stack, stack_vals->data[offset]); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KPOP) {
            knit_assert_s(insn->op1 > 0 && insn->op1 <= stack_vals->len, "popping too many values");

            for (int i=stack_vals->len - insn->op1; i < stack_vals->len; i++) {
                kdecref(stack_vals->data[i]);
            }
            rv = knitx_stack_rpop(knit, stack, insn->op1); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KCLOAD) {
            knit_assert_s(insn->op1 >= 0 && insn->op1 < block->constants.len, "loading out of range constant");
            rv = knitx_stack_rpush(knit, stack, block->constants.data[insn->op1]); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KLSTORE) {
            int dest = insn->op1 + top_frm->bsp ;
            knit_assert_s(dest >= 0 && dest < stack_vals->len, "");
            stack_vals->data[dest] = stack_vals->data[stack_vals->len - 1];
            rv = knitx_stack_rpop(knit, stack, 1); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KLLOAD) {
            int src = insn->op1 + top_frm->bsp;
            knit_assert_s(src >= 0 && src < stack_vals->len, "");
            rv = knitx_stack_rpush(knit, stack, stack_vals->data[src]); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == K_GLB_LOAD) {
            if (knitx_stack_ntemp(knit, &knit->ex.stack) < 1) {
                return knit_error(knit, KNIT_RUNTIME_ERR, "insufficent objects on the stack for global load");
            }
            /*inputs: (none)                       op: s[t-1] = globals[s[t-1]] */
            struct knit_obj *var_name = stack_vals->data[stack_vals->len - 1];
            rv = knitx_stack_rpop(knit, stack, 1); //pop name
            if (rv != KNIT_OK)
                return rv; 
            rv = knitx_do_global_load(knit, knit_as_str(var_name)); //value is pushed
        }
        else if (op == K_GLB_STORE) {
            /*inputs: (none)                       op: globals[s[t-2]] = s[t-1] */
            if (knitx_stack_ntemp(knit, &knit->ex.stack) < 2) {
                return knit_error(knit, KNIT_RUNTIME_ERR, "insufficent objects on the stack for global assignment");
            }
            struct knit_obj *lhs = stack_vals->data[stack_vals->len - 2];
            struct knit_obj *rhs = stack_vals->data[stack_vals->len - 1];
            //tmp global assumption
            rv = knitx_do_global_assign(knit, knit_as_str(lhs), rhs);
            rv = knitx_stack_rpop(knit, stack, 2);
        }
        else if (op == KCALL) {
            /*inputs: (nargs)       op: s[t-1](args...)*/
            
            struct knit_insn *next_insn = &block->insns.data[top_frm->u.kf.ip + 1];
            knit_assert_h(next_insn->insn_type == KCALLR, "");
            struct knit_obj *func = stack_vals->data[stack_vals->len - 1];
            int nargs = insn->op1;
            int nexpected_returns = next_insn->op1;
            //assuming cfunction
            //TODO: what to do with return values?
            //what happens at a call is, the returned values become at the top of the stack, the function and the passed arguments are popped
            if (func->u.ktype == KNIT_CFUNC) {
                rv = knitx_stack_push_frame_for_ccall(knit, (struct knit_cfunc *)func, nargs, nexpected_returns);
                knit->ex.nresults = -1;
                rv = func->u.cfunc.fptr(knit);
                if (knit->ex.nresults == -1) {
                    return knit_error(knit, KNIT_RUNTIME_ERR, "called C-function didn't declare returned values using knitx_creturns()");
                }
                if (knit->ex.nresults != nexpected_returns && nexpected_returns != KRES_UNKNOWN_KEEP_RET && nexpected_returns != KRES_UNKNOWN_DISCARD_RET) {
                    return knit_error(knit, KNIT_RUNTIME_ERR, "actual return values of called"
                                                              " C-function doesn't match the expected number,"
                                                              " expected %d got %d values", nexpected_returns, knit->ex.nresults);
                }
                int nreturns = knit->ex.nresults;
                /* Todo
                if (nexpected_returns == KRES_UNKNOWN_DISCARD_RET) {
                    nreturns = 0;
                }
                */
                top_frm = &frames->data[frames->len-1];
                int move_to = top_frm->bsp - 1 - top_frm->nargs; //because that's where the first passed argument is in (overwritten)
                knit_assert_h(move_to >= 0, "expected the function to return from to be pushed on the stack");
                knit_assert_h((stack_vals->len - move_to) >= nreturns, "returning too many values");
                rv = knitx_stack_moveup(knit, &knit->ex.stack, move_to, nreturns);
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_stack_pop_frame(knit, stack); 
                if (rv != KNIT_OK)
                    return rv;
                top_frm = &frames->data[frames->len-1];
                if (frames->len == 0) {
                    goto done; //end of execution
                }
                knit_assert_h(top_frm->bsp >= 0 && top_frm->bsp <= stack_vals->len, "");
            }
            else if (func->u.ktype == KNIT_KFUNC) {
                rv = knitx_stack_push_frame_for_kcall(knit, &func->u.kfunc.block, nargs, nexpected_returns);
                //it is executed in the loop
                top_frm = &frames->data[frames->len-1];
                block   = top_frm->u.kf.block;
                top_frm->u.kf.ip = -1; //undo the addition that happens at end of the loop
                //the rest is handled in KRET
            }
            else {
                return knit_error(knit, KNIT_RUNTIME_ERR, "tried to call a non-callable type") /*ml?*/;
            }

        }
        else if (op == KCALLR) {
            //no op, used by prev insn
        }
        else if (op == KINDX) {
            knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= 2, "no objects to index on");
            struct knit_obj *indexed = stack_vals->data[stack_vals->len - 2];
            struct knit_obj *index = stack_vals->data[stack_vals->len - 1];
            struct knit_obj *value = NULL;
            if (indexed->u.ktype == KNIT_LIST) {
                if (index->u.ktype != KNIT_INT) {
                    return knit_error(knit, KNIT_INVALID_TYPE_ERR, "trying to index using a type other than an int");
                }
                struct knit_list *list = (struct knit_list*) indexed;
                struct knit_int *idx = (struct knit_int*) index;
                if (idx->value < 0 || idx->value >= list->len) {
                    return knit_error(knit, KNIT_OUT_OF_RANGE_ERR, "index is out of range");
                }
                rv = knitx_stack_rpop(knit, stack, 2); 
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_stack_rpush(knit, stack, list->items[idx->value]); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else if (indexed->u.ktype == KNIT_DICT) {
                struct knit_dict *dict = (struct knit_dict*) indexed;
                struct knit_obj *value = NULL;
                rv = knitx_stack_rpop(knit, stack, 2); 
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_dict_lookup(knit, dict, index, &value); 
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_stack_rpush(knit, stack, value); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else {
                return knit_error(knit, KNIT_INVALID_TYPE_ERR, "trying to index a type other than lists/dicts");
            }
        }
        else if (op == KINDX_SET) {
            knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= 3, "insufficent objects on the stack to do array assignment");
            struct knit_obj *indexed = stack_vals->data[stack_vals->len - 3];
            struct knit_obj *index = stack_vals->data[stack_vals->len - 2];
            struct knit_obj *value = stack_vals->data[stack_vals->len - 1];
            if (indexed->u.ktype == KNIT_LIST) {
                if (index->u.ktype != KNIT_INT) {
                    return knit_error(knit, KNIT_INVALID_TYPE_ERR, "trying to index using a type other than an int");
                }
                struct knit_list *list = (struct knit_list*) indexed;
                struct knit_int *idx = (struct knit_int*) index;
                if (idx->value < 0 || idx->value >= list->len) {
                    return knit_error(knit, KNIT_OUT_OF_RANGE_ERR, "index is out of range");
                }
                list->items[idx->value] = value;
                rv = knitx_stack_rpop(knit, stack, 3); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else if (indexed->u.ktype == KNIT_DICT) {
                struct knit_dict *dict = (struct knit_dict*) indexed;
                rv = knitx_dict_set(knit, dict, index, value); 
                if (rv != KNIT_OK)
                    return rv;
                rv = knitx_stack_rpop(knit, stack, 3); 
                if (rv != KNIT_OK)
                    return rv;
            }
            else {
                return knit_error(knit, KNIT_INVALID_TYPE_ERR, "trying to index a type other than a list");
            }
        }
        else if (op == KDOT) {
            knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= 2, "no objects to index on");
            struct knit_obj *indexed = stack_vals->data[stack_vals->len - 2];
            struct knit_obj *index = stack_vals->data[stack_vals->len - 1];
            knit_assert_h(index->u.ktype == KNIT_STR, "expecting property name to be a string");

            struct knit_str *index_s = (struct knit_str *)index;

            struct knit_obj *prop = NULL;
            int rv = knitx_obj_get_property(knit, indexed, index_s, &prop); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_stack_rpop(knit, stack, 2); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_stack_rpush(knit, stack, prop); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KNLIST) {
            int nelements = insn->op1;
            int stacklen = stack_vals->len;
            struct knit_list *new_list = NULL;
            rv = knitx_list_new_gcobj(knit, &new_list, 4); 
            if (rv != KNIT_OK)
                return rv;

            knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= nelements, "");
            for (int i=stacklen - nelements; i < stacklen; i++) {
                rv = knitx_list_push(knit, new_list, stack_vals->data[i]);
                if (rv != KNIT_OK)
                    return rv;
            }
            rv = knitx_stack_rpush(knit, stack, (struct knit_obj *) new_list);  
            if (rv != KNIT_OK)
                return rv;
            //discard all elements that were moved to the list, and keep the list itself
            rv = knitx_stack_moveup(knit, &knit->ex.stack, stacklen - nelements, 1); 
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KNDICT) {
            struct knit_dict *new_dict = NULL;
            rv = knitx_dict_new_gcobj(knit, &new_dict, 4); 
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_stack_rpush(knit, stack, (struct knit_obj *) new_dict);  
            if (rv != KNIT_OK)
                return rv;
        }
        else if (op == KLIST_PUSH) {
            if (knitx_stack_ntemp(knit, &knit->ex.stack) < 2) {
                return knit_error(knit, KNIT_RUNTIME_ERR, "insufficent objects on the stack for list push");
            }
            rv = knitx_list_push(knit, knit_as_list(stack_vals->data[stack_vals->len - 2]), stack_vals->data[stack_vals->len - 1]);
            if (rv != KNIT_OK)
                return rv;
            rv = knitx_stack_rpop(knit, stack, 1);
        }
        else if (op == KRET) {
            int nreturns = insn->op1;
            int nexpected_returns = top_frm->nexpected_returns;
            if (nreturns != nexpected_returns && nexpected_returns != KRES_UNKNOWN_KEEP_RET && nexpected_returns != KRES_UNKNOWN_DISCARD_RET) {
                return knit_error(knit, KNIT_RUNTIME_ERR, "actual return values of called"
                                                            " function doesn't match the expected number,"
                                                            " expected %d got %d values", nexpected_returns, nreturns);
            }
            int move_to = top_frm->bsp - 1 - top_frm->nargs; //because that's where the first passed argument is in (overwritten)
            knit_assert_h(move_to >= 0, "expected the function to return from to be pushed on the stack");
            knit_assert_h((stack_vals->len - move_to) >= nreturns, "returning too many values");
            rv = knitx_stack_moveup(knit, &knit->ex.stack, move_to, nreturns);
            knit->ex.nresults = nreturns;
            rv = knitx_stack_pop_frame(knit, stack); 
if (rv != KNIT_OK)
    return rv;
            if (frames->len == 0) {
                goto done; //end of execution
            }
            top_frm = &frames->data[frames->len-1];
            block = top_frm->u.kf.block;

            knit_assert_h(top_frm->frame_type == KNIT_FRAME_KBLOCK, "");
            knit_assert_h(top_frm->bsp >= 0 && top_frm->bsp <= stack_vals->len, "");
        }
        else if (op == KJMP) {
            top_frm->u.kf.ip = insn->op1 - 1; 
        }
        else if (op == KJMPTRUE) {
            if (knit->ex.last_cond)
                top_frm->u.kf.ip = insn->op1 - 1; 
        }
        else if (op == KJMPFALSE) {
            if (!knit->ex.last_cond)
                top_frm->u.kf.ip = insn->op1 - 1; 
        }
        else if (op == KTESTEQ || op == KTESTNEQ  || op == KTESTGT  ||
                 op == KTESTLT || op == KTESTGTEQ || op == KTESTLTEQ  )
        {
            rv = knitx_op_exec_test_binop(knit, stack, op);
        }
        else if (op == KTESTNOT || op == KTEST ) {
            knit_assert_h(knitx_stack_ntemp(knit, &knit->ex.stack) >= 1, "insufficent objects on the stack for KTEST/KTESTNOT");
            struct knit_obj *obj = stack_vals->data[stack_vals->len - 1];
            rv = knitx_test_bool(knit, obj);
            if (op == KTESTNOT)
                knit->ex.last_cond = !knit->ex.last_cond;
            rv = knitx_stack_rpop(knit, stack, 1);
        }
        else if (op == KEMIT) {
            switch (insn->op1) {
                case KEMTRUE:  rv = knitx_stack_rpush(knit, stack, (struct knit_obj *) &ktrue);  break;
                case KEMFALSE: rv = knitx_stack_rpush(knit, stack, (struct knit_obj *) &kfalse); break;
                case KEMNULL:  rv = knitx_stack_rpush(knit, stack, (struct knit_obj *) &knull);  break;
                default: return knit_runtime_error(knit, "insn's op1 not expected: %s", knit_insn_name(op));
            }
        }
        else if (op == KSAVETEST) {
            rv = knitx_stack_rpush(knit, stack, (struct knit_obj *)(knit->ex.last_cond ? &ktrue : &kfalse));
        }
        else if (op == KNEG) {
            struct knit_obj *obj = stack_vals->data[stack_vals->len - 1];
            if (obj->u.ktype != KNIT_INT) {
                return knit_error(knit, KNIT_INVALID_TYPE_ERR, "trying to negatie a type other than an int");
            }
            struct knit_int *iobj = (struct knit_int *)obj;
            int val = - iobj->value;
            struct knit_int *ri = NULL;
            rv = knitx_int_new_gcobj(knit, &ri, val);
            rv = knitx_stack_rpop(knit, stack, 1); //we can't mutate directly, because something else might be referring to it
            rv = knitx_stack_rpush(knit, stack, (struct knit_obj *)ri);
        }
        else if (op == KNOT) {
            struct knit_obj *obj = stack_vals->data[stack_vals->len - 1];
            rv = knitx_test_bool(knit, obj); 
            if (rv != KNIT_OK)
                return rv;
            int boolean = !knit->ex.last_cond;
            rv = knitx_stack_rpop(knit, stack, 1);
            rv = knitx_stack_rpush(knit, stack, (struct knit_obj *)(boolean ? &ktrue : &kfalse));
        }
        else if (op == KNOP) {
            //no operation
        }
        else if (op == KADD || op == KSUB || op == KMUL || op == KDIV || op == KMOD) {
            rv = knitx_op_exec_binop(knit, stack, op);
        }
        else {
            return knit_runtime_error(knit, "insn not supported: %s", knit_insn_name(op));
        }
        if (rv != KNIT_OK) {
            //TODO handle error by returning from function
            return rv;
        }
        top_frm->u.kf.ip++; //we have no support for branches currently
    }
done:
    return KNIT_OK;
}

static int knitx_block_exec(struct knit *knit, struct knit_block *block, int nargs, int nexpret) {
    int rv = knitx_stack_rpush(knit, &knit->ex.stack, (struct knit_obj *)(&knull)); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_stack_push_frame_for_kcall(knit, block, nargs, nexpret); 
    if (rv != KNIT_OK)
        return rv;
    rv = knitx_exec(knit); 
    if (rv != KNIT_OK)
        return rv;
    return KNIT_OK;
}

static int knitx_exec_str(struct knit *knit, const char *program) {
    struct knit_prs prs;
    int rv = KNIT_OK;

#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_prs_init1(knit, &prs);
        knitx_lexer_init_str(knit, &prs.lex, program);
        knitx_lexdump(knit, &prs.lex);
        knitx_lexer_deinit(knit, &prs.lex);
        knitx_prs_deinit(knit, &prs);
    }
#endif 

    knitx_prs_init1(knit, &prs);
    knitx_lexer_init_str(knit, &prs.lex, program);
    knitx_prog(knit, &prs);

#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_block_dump(knit, &prs.curblk->block);
    }
#endif

    knitx_block_exec(knit, &prs.curblk->block, 0, 0);

#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_stack_dump(knit, &knit->ex.stack, -1, -1);
    }
#endif

    knitx_lexer_deinit(knit, &prs.lex);
    knitx_prs_deinit(knit, &prs);

    return KNIT_OK; //dummy
}

//how many chars to skip an escape seq
static int knit_strliteral_skip_len(const char *buf, int len) {
    if (buf[0] == '\\') 
        return 2;
    return 1; //no escape
}

//in repl, this is used to query whether the thing can be executed
//return value: 0 -> can't
//an index to which we seem able to execute, the rest must be saved by the caller and not passed to be executed
static int knit_can_exec(const char *buf, int len) {
    int pbcd = 0;
    //TODO this doesnt work with quoted strings
    int maxto = 0;
    int instr = 0;
    int i =0;
    for (i=0; i<len; i++) {
        if (buf[i] == '\'' || buf[i] == '"') {
           instr = !instr;
        }
        else {
            if (instr) {
                int sl = knit_strliteral_skip_len(buf+i, len-i);
                i += (sl - 1); //- 1 because the loop adds 1
            }
            else {
                switch(buf[i]) {
                    case '[': pbcd++; break;
                    case ']': pbcd--; break;
                    case '{': pbcd++; break;
                    case '}': pbcd--; break;
                    case '(': pbcd++; break;
                    case ')': pbcd--; break;
                    case ' ': case '\t': case '\v': case '\f': case '\r': continue /*the for loop*/;
                }
            }
            if (pbcd == 0 && buf[i] == '\n') {
                maxto = i + 1;
            }
        }
    }
    if (pbcd == 0) {
        maxto = len;
    }
    return maxto;
}

static int knitx_init(struct knit *knit, int opts) {
    if (opts & KNIT_POLICY_CONTINUE)
        knit_set_error_policy(knit, KNIT_POLICY_CONTINUE);
    else 
        knit_set_error_policy(knit, KNIT_POLICY_EXIT);
#ifdef KNIT_MEM_STATS
    knit_mem_stats_init(&knit->mstats);
#endif
    knit->ex.nresults = 0;
    knit->err_msg = NULL;
    knit->err = KNIT_OK;
    return knitx_exec_state_init(knit, &knit->ex);
}

static void knit_obj_deinit(struct knit *knit, struct knit_obj *obj) {
    switch (obj->u.ktype) {
        case KNIT_NULL: break;
        case KNIT_STR : knitx_str_deinit(knit, (struct knit_str *)obj); break;
        case KNIT_LIST: knitx_list_deinit(knit, (struct knit_list *) obj); break;
        case KNIT_DICT: knitx_dict_deinit(knit, (struct knit_dict *) obj); break;
        case KNIT_INT: break;
        case KNIT_CFUNC: break;
        case KNIT_KFUNC: knit_kfunc_deinit(knit, (struct knit_kfunc *)obj); break;
        case KNIT_TRUE: break;
        case KNIT_FALSE: break;
        default: knit_assert_h(0, "invalid type");
    }
}

static int knitx_deinit(struct knit *knit) {
    return KNIT_OK;
}

#include "kruntime.h" //runtime functions
#endif
