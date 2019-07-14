#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> //need uintptr_t
#include <stdarg.h>
#include <stdio.h>
//there other includes in the file. 

#include "kdata.h" //data structures

//enable checks that are not needed for purposes other than testing
#define KNIT_CHECKS

static void knit_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
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

//fwd
static int knit_error(struct knit *knit, int err_type, const char *fmt, ...);

//TODO: make sure no leaks occur during failures of these functions
//      make sure error reporting works in low memory conditions
static int knitx_register_new_ptr_(struct knit *knit, void *m) {
    unsigned char v = 0;
    int rv = mem_hasht_insert(&knit->mem_ht, &m, &v);
    if (rv != MEM_HASHT_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_register_new_ptr_(): mem_hasht_insert() failed");
    }
    return KNIT_OK;
}
static int knitx_unregister_ptr_(struct knit *knit, void *m) {
    unsigned char v = 0;
    int rv = mem_hasht_remove(&knit->mem_ht, &m);
    if (rv != MEM_HASHT_OK) {
        if (rv == MEM_HASHT_NOT_FOUND) {
            return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_remove_ptr_(): key not found, trying to unregister an unregistered pointer");
        }
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_remove_ptr_(): hasht_find() failed");
    }
    return KNIT_OK;
}
static int knitx_update_ptr_(struct knit *knit, void *old_m, void *new_m) {
    if (old_m == new_m)
        return KNIT_OK;
    int rv = KNIT_OK;
    if (old_m != NULL)
        rv = knitx_unregister_ptr_(knit, old_m);
    if (rv != KNIT_OK)
        return rv;
    if (new_m == NULL)
        return KNIT_OK;
    return knitx_register_new_ptr_(knit, new_m);
}

//untracked allocation
static int knitx_rfree(struct knit *knit, void *p) {
    (void) knit;
    free(p);
    return KNIT_OK;
}
static int knitx_rmalloc(struct knit *knit, size_t sz, void **m) {
    knit_assert_h(sz, "knit_malloc(): 0 size passed");
    void *p = malloc(sz);
    *m = NULL;
    if (!p)
        return knit_error(knit, KNIT_NOMEM, "knitx_malloc(): malloc() returned NULL");
    *m = p;
    return KNIT_OK;
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
static int knitx_rrealloc(struct knit *knit, void *p, size_t sz, void **m) {
    if (!sz) {
        int rv = knitx_rfree(knit, p);
        *m = NULL;
        return rv;
    }
    void *np = realloc(p, sz);
    if (!np) {
        return knit_error(knit, KNIT_NOMEM, "knitx_realloc(): realloc() returned NULL");
    }
    *m = np;
    return KNIT_OK;
}

//tracked allocation
static int knitx_tmalloc(struct knit *knit, size_t sz, void **m) {
    int rv = knitx_rmalloc(knit, sz, m);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_register_new_ptr_(knit, *m);
    if (rv != KNIT_OK) {
        knitx_rfree(knit, *m);
        return rv;
    }
    return KNIT_OK;
}
static int knitx_tfree(struct knit *knit, void *p) {
    if (!p)
        return KNIT_OK;
    int rv = knitx_unregister_ptr_(knit, p);
    knitx_rfree(knit, p);
    return rv;
}
static int knitx_trealloc(struct knit *knit, void *p, size_t sz, void **m) {
    int rv = knitx_rrealloc(knit, p, sz, m);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_update_ptr_(knit, p, *m);
    return rv;
}


static int knitx_list_init(struct knit *knit, struct knit_list *list) {
    knit_fatal("currently not implemented");
    return KNIT_OK;
}
static int knitx_list_deinit(struct knit *knit, struct knit_list *list) {
    knit_fatal("currently not implemented");
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

static int knitx_int_new(struct knit *knit, struct knit_int **integerp_out, int value) {
    void *p = NULL;
    int rv = knitx_tmalloc(knit, sizeof(struct knit_int), &p);
    if (rv != KNIT_OK) {
        *integerp_out = NULL;
        return rv;
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


static int knitx_str_init(struct knit *knit, struct knit_str *str) {
    (void) knit;
    str->ktype = KNIT_STR;
    str->str = NULL;
    str->cap = 0;
    str->len = 0;
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
static int knitx_str_destroy(struct knit *knit, struct knit_str *strp) {
    int rv = knitx_str_deinit(knit, strp);
    int rv2 = knitx_tfree(knit, strp);
    if (rv != KNIT_OK)
        return rv;
    return rv2;
}
static int knitx_str_strcpy(struct knit *knit, struct knit_str *str, const char *src0);
static int knitx_str_new_strcpy(struct knit *knit, struct knit_str **strp, const char *src0) {
    int rv = knitx_str_new(knit, strp);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knitx_str_strcpy(knit, *strp, src0);
    if (rv != KNIT_OK) {
        knitx_str_destroy(knit, *strp);
        *strp = NULL;
        return rv;
    }
    return KNIT_OK;
}

static int knitx_str_clear(struct knit *knit, struct knit_str *str) {
    (void) knit;
    if (str->cap >= 0) {
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
        void *p;
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
    struct vars_hasht_iter iter;
    rv = vars_hasht_find(&knit->vars_ht, &key, &iter);
    if (rv != VARS_HASHT_OK) {
        if (rv == VARS_HASHT_NOT_FOUND) 
            return knit_error(knit, KNIT_NOT_FOUND_ERR, "variable '%s' is undefined", varname);
        else 
            return knit_error(knit, KNIT_RUNTIME_ERR, "an error occured while trying to lookup a variable in vars_hasht_find()");
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
            printf("'%s'", valp->str);
        }
        else if (valpo->u.ktype == KNIT_LIST) {
            struct knit_list *valp = &valpo->u.list;
            printf("LIST");
        }
    }
    else {
        printf("NULL");
    }
    printf("\n");
    return rv;
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
        abort();
        exit(1);
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
    rv = knitx_str_new(knit, &val_strp);
    if (rv != KNIT_OK) 
        goto cleanup_key;
    rv = knitx_str_strcpy(knit, val_strp, value);
    if (rv != KNIT_OK) 
        goto cleanup_val;
    //ownership of key is transferred to the vars hashtable
    struct knit_obj *objp = (struct knit_obj *) val_strp; //defined operation?
    rv = vars_hasht_insert(&knit->vars_ht, &key_str, &objp);
    if (rv != VARS_HASHT_OK) {
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

struct knit_lex; //fwd
struct knit_prs; //fwd
static int knitx_lexer_peek_cur(struct knit *knit, struct knit_lex *lxr, struct knit_tok **tokp);
static int knitx_lexer_peek_la(struct knit *knit, struct knit_lex *lxr, struct knit_tok **tokp);
static int knitx_knit_tok_set_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *str);

static int knitx_lexer_init(struct knit *knit, struct knit_lex *lxr) {
    lxr->lineno = 1;
    lxr->colno = 1;
    lxr->offset = 0;
    lxr->tokno = 0;
    int rv = tok_darr_init(&lxr->tokens, 100);
    return rv;
}
static int knitx_lexer_deinit(struct knit *knit, struct knit_lex *lxr); //fwd
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
    tok_darr_deinit(&lxr->tokens);
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
 *          
 *
 *
  */

//check condition or return rt err
#define KNIT_CCND(cnd) \
    do { \
        if (!(cnd)) { \
            return KNIT_RUNTIME_ERR; \
        } \
    } while(0)
//check condition or goto
#define KNIT_CCND_JMP(cnd, label) \
    do { \
        if (!(cnd)) { \
            goto label; \
        } \
    } while(0)
//check return value or return
#define KNIT_CRV(rv) \
    do { \
        if ((rv) != KNIT_OK) \
            return (rv); \
    } while (0) 

//check return value or goto
#define KNIT_CRV_JMP(rv, label) \
    do { \
        if ((rv) != KNIT_OK) \
            goto label; \
    } while (0) 


static int knitx_block_init(struct knit *knit, struct knit_block *block) {
    int rv = insns_darr_init(&block->insns, 256);
    if (rv != INSNS_DARR_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_init(): initializing statements darray failed");
    }
    rv = knit_objp_darr_init(&block->constants, 128); KNIT_CRV_JMP(rv, fail_objp_darr);
    return KNIT_OK;

fail_objp_darr:
    insns_darr_deinit(&block->insns);
    return rv;
}
static int knitx_block_add_insn(struct knit *knit, struct knit_block *block, struct knit_insn *insn) {
    int rv = insns_darr_push(&block->insns, insn);
    if (rv != INSNS_DARR_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_add_insn(): adding a insn to insns darray failed");
    }
    return KNIT_OK;
}
//block owns allocd_obj, it is expected to be a tmallocd ptr (TODO check)
static int knitx_block_add_constant(struct knit *knit, struct knit_block *block, struct knit_obj *allocd_obj, int *index_out) {
    int rv = knit_objp_darr_push(&block->constants, &allocd_obj);
    if (rv != KNIT_OBJP_DARR_OK) {
        *index_out = -1;
        return knit_error(knit, KNIT_RUNTIME_ERR, "knitx_block_add_constant(): adding a constant to darray failed");
    }
    knit_assert_s(block->constants.len > 0, "darray push silently failed");
    *index_out = block->constants.len - 1;
    return KNIT_OK;
}
static int knitx_block_deinit(struct knit *knit, struct knit_block *block) {
    insns_darr_deinit(&block->insns);
    knitx_block_init(knit, block);
    return KNIT_OK;
}
//outi_str must be already initialized
static int knitx_obj_rep(struct knit *knit, struct knit_obj *obj, struct knit_str *outi_str) {
    int rv = KNIT_OK;
    if (obj == NULL) {
        rv = knitx_str_strcpy(knit, outi_str, "[NULL]"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_STR) {
        struct knit_str *objstr = (struct knit_str *) obj;
        rv = knitx_str_strlcpy(knit, outi_str, "\"", 1); KNIT_CRV(rv);
        rv = knitx_str_strlappend(knit, outi_str, objstr->str, objstr->len); KNIT_CRV(rv);
        rv = knitx_str_strlappend(knit, outi_str, "\"", 1); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_INT) {
        struct knit_int *objint = (struct knit_int *) obj;
        knit_sprintf(knit, outi_str, "%d", objint->value);
    }
    else if (obj->u.ktype == KNIT_LIST) {
        struct knit_list *objlist = (struct knit_list *) obj;
        rv = knitx_str_strlcpy(knit, outi_str, "[", 1); KNIT_CRV(rv);
        for (int i=0; i<objlist->len; i++) {
            if (i) {
                rv = knitx_str_strlappend(knit, outi_str, ", ", 2); KNIT_CRV(rv);
            }
            struct knit_str *tmpstr = NULL;
            rv = knitx_str_new(knit, &tmpstr); KNIT_CRV(rv);
            rv = knitx_obj_rep(knit, objlist->items[i], tmpstr);
            if (rv != KNIT_OK) {
                knitx_str_destroy(knit, tmpstr);
                return rv;
            }
            rv = knitx_str_strlappend(knit, outi_str, tmpstr->str, tmpstr->len); 
            knitx_str_destroy(knit, tmpstr);
            KNIT_CRV(rv);
        }
        rv = knitx_str_strlappend(knit, outi_str, "]", 1); KNIT_CRV(rv);
        KNIT_CRV(rv);
    }
    else {
        knit_fatal("knitx_obj_rep(): unknown object type");
    }
    return KNIT_OK;
}
static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj) {
    struct knit_str *tmpstr = NULL;
    int rv = knitx_str_new(knit, &tmpstr); KNIT_CRV(rv);
    rv = knitx_obj_rep(knit, obj, tmpstr);
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
    int rv = knitx_str_init(knit, &str); KNIT_CRV(rv);
    for (int i=0; i<block->constants.len; i++) {
        rv = knitx_obj_rep(knit, block->constants.data[i], &str);
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
    return knit_sprintf(knit, outi_str, "[block %p (c: %d, i: %d)]", (void *) block, block->constants.len, block->insns.len);
}
static int knitx_block_dump(struct knit *knit, struct knit_block *block) { 
    knitx_block_dump_consts(knit, block);
    fprintf(stderr, "Instructions:\n");
    for (int i=0; i<block->insns.len; i++) {
        struct knit_insn *insn = &block->insns.data[i];
        struct knit_insninfo *inf = &knit_insninfo[insn->insn_type];
        fprintf(stderr, "\t%d\t%s", i, inf->rep);
        switch (inf->n_op) {
            case 2: printf(" %d,", insn->op2);/*fall through*/
            case 1: printf(" %d",  insn->op1);/*fall through*/
            case 0: printf("\n"); break; 
            default: knit_fatal("knitx_block_dump(): invalid no. operands"); break;
        }
    }
    return KNIT_OK;
}

#define kincref(p) knitx_obj_incref(knit, (struct knit_obj *)(p))
#define kdecref(p) knitx_obj_decref(knit, (struct knit_obj *)(p))
#define ktobj(p) ((struct knit_obj *) (p))

/*EXECUTION STATE FUNCS*/
static int knitx_frame_init(struct knit *knit, struct knit_frame *frame, struct knit_block *block, int ip, int bsp) {
    kincref(block);
    frame->block = block;
    frame->ip = ip;
    frame->bsp = bsp;
    return KNIT_OK;
}
static int knitx_frame_deinit(struct knit *knit, struct knit_frame *frame) {
    kdecref(frame->block);
    return KNIT_OK;
}
//outi_str must be already initialized
static int knitx_frame_rep(struct knit *knit, struct knit_frame *frame, struct knit_str *outi_str) {
    int rv = KNIT_OK;
    struct knit_str *tmpstr = NULL;
    rv = knitx_str_new(knit, &tmpstr); KNIT_CRV(rv);
    rv = knitx_block_rep(knit, frame->block, tmpstr);
    if (rv != KNIT_OK) {
        goto cleanup_tmpstr;
    }
    rv = knit_sprintf(knit, outi_str, "[frame, block: %s, ip: %d, bsp: %d]", tmpstr->str, frame->ip, frame->bsp);
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
    int rv = knit_frame_darr_init(&stack->frames, 128);
    if (rv != KNIT_FRAME_DARR_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_init(): initializing frames stack failed");
    }
    rv = knit_objp_darr_init(&stack->vals, 512);
    if (rv != KNIT_OBJP_DARR_OK) {
        knit_frame_darr_deinit(&stack->frames);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_init(): initializing values stack failed");
    }
    return KNIT_OK;
}
static int knitx_stack_deinit(struct knit *knit, struct knit_stack *stack) {
    int rv1 = knit_objp_darr_deinit(&stack->vals);
    int rv  = knit_frame_darr_deinit(&stack->frames);
    if (rv1 != KNIT_OBJP_DARR_OK || rv != KNIT_FRAME_DARR_OK)
        return KNIT_DEINIT_ERR;
    return KNIT_OK;
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
    rv = knitx_str_new(knit, &tmpstr1); KNIT_CRV(rv);
    rv = knitx_str_new(knit, &tmpstr2); KNIT_CRV_JMP(rv, cleanup_tmpstr1);

    fprintf(stderr, "Stack Values:\n");
    for (int i=stack->vals.len - n_values; i < stack->vals.len; i++) {
        rv = knitx_obj_rep(knit, stack->vals.data[i], tmpstr1);                             KNIT_CRV_JMP(rv, cleanup_tmpstr2);
        rv = knit_sprintf(knit, tmpstr2, "\ts[%d]: %s\n", i, tmpstr1->str, tmpstr1->len);   KNIT_CRV_JMP(rv, cleanup_tmpstr2);
        /* rv = knitx_str_strlappend(knit, outi_str, tmpstr2->str, tmpstr2->len);              KNIT_CRV_JMP(rv, cleanup_tmpstr2); */
        fprintf(stderr, "%s", tmpstr2->str);
    }
    fprintf(stderr, "Backtrace:\n");
    for (int i=stack->frames.len - n_backtrace; i < stack->frames.len; i++) {
        rv = knitx_frame_rep(knit, &stack->frames.data[i], tmpstr1);                            KNIT_CRV_JMP(rv, cleanup_tmpstr2);
        rv = knit_sprintf(knit, tmpstr2, "\tf[%d]: %s\n", i, tmpstr1->str);    KNIT_CRV_JMP(rv, cleanup_tmpstr2);
        /* rv = knitx_str_strlappend(knit, outi_str, tmpstr2->str, tmpstr2->len); KNIT_CRV_JMP(rv, cleanup_tmpstr2); */
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
static int knitx_stack_push_frame_for_call(struct knit *knit, struct knit_block *block) {
    struct knit_frame frm;
    int bsp = knit->ex.stack.vals.len; //base stack pointer
    int rv = knitx_frame_init(knit, &frm, block, 0, bsp);
    if (rv != KNIT_OK) {
        return rv;
    }
    struct knit_exec_state *exs = &knit->ex;
    rv = knit_frame_darr_push(&exs->stack.frames, &frm);
    if (rv != KNIT_FRAME_DARR_OK) {
        knitx_frame_deinit(knit, &frm);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_push_frame_for_call(): pushing a stack frame failed");
    }
    kincref(block);
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
    return knitx_stack_init(knit, &exs->stack);
}
static int knitx_exec_state_deinit(struct knit *knit, struct knit_exec_state *exs) {
    return knitx_stack_deinit(knit, &exs->stack);
}
/*END OF EXECUTION STATE FUNCS*/

/*
    some of the syntax is based on LUA's syntax specification
    {A} means 0 or more As, and [A] means an optional A.

	program ::= block

    program -> block
    block -> stat
    stat -> var 
    stat -> functioncall
    stat -> ';'

    var  ->  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name 
                    functioncall |
                    ‘(’ exp ‘)’




    symbol          first
    program         Name + '(' 
    stat            Name + '(' 
    var             Name + '('
    prefixexp       Name + '('
    exp             null + false + Numeral + LiteralString + '[' + '(' + Name + f(unop)
    literallist     '['


	block ::= {stat} 

	stat ::=  ‘;’ | 
		 var ‘=’ exp | 
		 functioncall 

	prefixexp ::= var | functioncall | ‘(’ exp ‘)’

	exp ::=  null | false | true | Numeral | LiteralString | literallist | prefixexp |  exp binop exp | unop exp 

    LiteralString ::= "[^"]+"
    Name ::=  [a-zA-Z_][a-zA-Z0-9_]*
    literallist ::= '[' explist ','? ']' 

	varlist ::= var {‘,’ var}

	var ::=  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name 

	namelist ::= Name {‘,’ Name}

	explist ::= exp {‘,’ exp}

	functioncall ::=  prefixexp args 

	args ::=  ‘(’ [explist] ‘)’ 

	binop ::=  ‘+’ | ‘-’ | ‘*’ | ‘/’ | ‘//’ | ‘^’ | ‘%’ | 
		 ‘&’ | ‘~’ | ‘|’ | ‘>>’ | ‘<<’ | ‘..’ | 
		 ‘<’ | ‘<=’ | ‘>’ | ‘>=’ | ‘==’ | ‘!=’ | 
		 and | or | not

	unop ::= ‘-’ | not | ‘#’ | ‘~’

 * */



static int knitx_lexer_init_str(struct knit *knit, struct knit_lex *lxr, const char *program);
static int knitx_lexer_deinit(struct knit *knit, struct knit_lex *lxr);

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
    int rv = tok_darr_push(&lxr->tokens, &tok);
    if (rv != TOK_DARR_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "couldn't push token to tokens buffer");
    }
    return KNIT_OK;
}

/*LEXICAL ANALYSIS FUNCS*/

static int knitx_lexer_add_strliteral(struct knit *knit, struct knit_lex *lxr) {
    lxr->offset++; //skip '
    int beg = lxr->offset;
    while (lxr->offset < lxr->input->len && lxr->input->str[lxr->offset] != '\'') {
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
    int rv = knitx_lexer_add_tok(knit, lxr, KAT_STRLITERAL, beg, lxr->offset - beg, lxr->lineno, lxr->colno, NULL);
    lxr->offset++; //skip '
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
    return knitx_lexer_add_tok(knit, lxr, KAT_VAR, beg, lxr->offset - beg, lxr->lineno, lxr->colno, NULL);
}
static int knitx_lexer_skip_wspace(struct knit *knit, struct knit_lex *lxr) {
    while (lxr->offset < lxr->input->len && 
                (lxr->input->str[lxr->offset] == ' '  ||
                 lxr->input->str[lxr->offset] == '\t' ||
                 lxr->input->str[lxr->offset] == '\n' ||
                 lxr->input->str[lxr->offset] == '\r' ||
                 lxr->input->str[lxr->offset] == '\v' ||
                 lxr->input->str[lxr->offset] == '\f'   )) 
    {
        lxr->offset++;
    }
    return KNIT_OK;
}
static int knitx_lexer_lex(struct knit *knit, struct knit_lex *lxr) {
    int rv = KNIT_OK;
again:
    if (lxr->offset >= lxr->input->len) {
        if (!lxr->tokens.len || lxr->tokens.data[lxr->tokens.len-1].toktype != KAT_EOF) {
            return knitx_lexer_add_tok(knit, lxr, KAT_EOF, lxr->offset, 0, lxr->lineno, lxr->colno, NULL);
        }
        return KNIT_OK;
    }
    switch (lxr->input->str[lxr->offset]) {
        case ';':   rv = knitx_lexer_add_tok(knit, lxr, KAT_SEMICOLON, lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ':':   rv = knitx_lexer_add_tok(knit, lxr, KAT_COLON,     lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '.':   rv = knitx_lexer_add_tok(knit, lxr, KAT_DOT,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ',':   rv = knitx_lexer_add_tok(knit, lxr, KAT_COMMA,     lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '=':   rv = knitx_lexer_add_tok(knit, lxr, KAT_EQUALS,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '[':   rv = knitx_lexer_add_tok(knit, lxr, KAT_OBRACKET,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ']':   rv = knitx_lexer_add_tok(knit, lxr, KAT_CBRACKET,  lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '(':   rv = knitx_lexer_add_tok(knit, lxr, KAT_OPAREN,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case ')':   rv = knitx_lexer_add_tok(knit, lxr, KAT_CPAREN,    lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '+':   rv = knitx_lexer_add_tok(knit, lxr, KAT_ADD,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '-':   rv = knitx_lexer_add_tok(knit, lxr, KAT_SUB,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '*':   rv = knitx_lexer_add_tok(knit, lxr, KAT_MUL,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '/':   rv = knitx_lexer_add_tok(knit, lxr, KAT_DIV,       lxr->offset, 1, lxr->lineno, lxr->colno, NULL); lxr->offset += 1; break;
        case '\'':  rv = knitx_lexer_add_strliteral(knit, lxr); break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': /* FALLTHROUGH */
                    rv = knitx_lexer_add_intliteral(knit, lxr); break;
        case ' ': case '\t': case '\v': case '\f': case '\r': case '\n': /* FALLTHROUGH */
                    rv = knitx_lexer_skip_wspace(knit, lxr); goto again; break;
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
static int knitx_lexer_adv(struct knit *knit, struct knit_lex *lxr) {
    int rv = KNIT_OK;
    knit_assert_h(lxr->tokno < lxr->tokens.len, "knitx_lexer_adv(): invalid .tokno");
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
    knit_assert_h(lxr->tokno + 1 < lxr->tokens.len, "");
    *tokp = &lxr->tokens.data[lxr->tokno];
    return KNIT_OK;
}
static int knitx_lexer_curtoktype(struct knit *knit, struct knit_prs *prs) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_la(knit, lxr, &tokp);
    if (rv != KNIT_OK)
        return KAT_EOF;
    return tokp->toktype;
}
static int knitx_lexer_latoktype(struct knit *knit, struct knit_prs *prs) {
    struct knit_lex *lxr = &prs->lex;
    struct knit_tok *tokp;
    int rv = knitx_lexer_peek_cur(knit, lxr, &tokp);
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
static int knitx_knit_tok_set_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *outi_str) {
    knit_assert_h(!!tok && !!outi_str, "");
    knit_assert_h(tok->offset < lxr->input->len, "");
    int rv = knitx_str_strlcpy(knit, outi_str, lxr->input->str + tok->offset, tok->len);
    return rv;
}
static const char *knit_knit_tok_name(int name) {
    static const struct { const int type; const char * const rep } toktypes[] = {
        {KAT_EOF, "KAT_EOF"},
        {KAT_BOF, "KAT_BOF"}, 
        {KAT_STRLITERAL, "KAT_STRLITERAL"},
        {KAT_INTLITERAL, "KAT_INTLITERAL"},
        {KAT_VAR, "KAT_VAR"},
        {KAT_DOT, "KAT_DOT"},
        {KAT_EQUALS, "KAT_EQUALS"},
        {KAT_OPAREN, "KAT_OPAREN"},
        {KAT_CPAREN, "KAT_CPAREN"},
        {KAT_ADD, "KAT_ADD"},
        {KAT_SUB, "KAT_SUB"},
        {KAT_MUL, "KAT_MUL"},
        {KAT_DIV, "KAT_DIV"},
        {KAT_OBRACKET, "KAT_OBRACKET"},
        {KAT_CBRACKET, "KAT_CBRACKET"},
        {KAT_COMMA, "KAT_COMMA"},
        {KAT_COLON, "KAT_COLON"},
        {KAT_SEMICOLON, "KAT_SEMICOLON"},
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
    knit_assert_h(!!tok && !!str, "");
    knit_assert_h(tok->offset < lxr->input->len, "");
    struct knit_str *tmp0;
    struct knit_str *tmp1;
    int rv = knitx_str_new(knit, &tmp0);
    if (rv != KNIT_OK) 
        goto end;
    rv = knitx_str_new(knit, &tmp1);
    if (rv != KNIT_OK)
        goto cleanup_tmp0;
    rv = knitx_knit_tok_set_str(knit, lxr, tok, tmp0);
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
        printf("\tToken %d: %s\n", lxr->tokno, tokstr->str);
        rv = knitx_lexer_adv(knit, lxr);
        if (rv != KNIT_OK)
            break;
    }
    knitx_str_destroy(knit, tokstr);
    return rv;
}

/*END OF LEXICAL ANALYSIS FUNCS*/
/*PARSING FUNCS*/

//should do prs init then do lex init (broken?)
static int knitx_prs_init(struct knit *knit, struct knit_prs *prs) {
    memset(prs, 0, sizeof(*prs));
    int rv = knitx_block_deinit(knit, &prs->block); KNIT_CRV(rv);
    return KNIT_OK;
}
static int knitx_prs_deinit(struct knit *knit, struct knit_prs *prs) {
    knitx_block_deinit(knit, &prs->block);
    return KNIT_OK;
}


static int knit_error_expected(struct knit *knit, struct knit_prs *prs, int expected, const char *msg) {
    struct knit_str str1;
    int rv = knitx_str_init(knit, &str1); KNIT_CRV(rv);
    struct knit_tok *tok;
    rv = knitx_lexer_peek_cur(knit, &prs->lex, &tok); KNIT_CRV_JMP(rv, fail_1);
    rv = knitx_knit_tok_repr_set_str(knit, &prs->lex, tok, &str1); KNIT_CRV_JMP(rv, fail_1);
    const char *exstr = knit_knit_tok_name(expected);
    rv = knit_error(knit, KNIT_SYNTAX_ERR, "expected %s got %s", exstr, str1.str);
fail_1: knitx_str_deinit(knit, &str1);
    return rv;
}

//returns NULL on error
static struct knit_tok *knitx_lexer_curtok(struct knit *knit, struct knit_prs *prs) {
    static struct knit_tok *tokp;
    int rv = knitx_lexer_peek_cur(knit, &prs->lex, &tokp);
    if (rv != KNIT_OK) {
        return NULL;
    }
    return tokp;
}
static struct knit_tok *knitx_lexer_latok(struct knit *knit, struct knit_prs *prs) {
    static struct knit_tok *tokp;
    int rv = knitx_lexer_peek_la(knit, &prs->lex, &tokp);
    if (rv != KNIT_OK) {
        return NULL;
    }
    return tokp;
}

#define K_CURR_TOKTYPE()    (knitx_lexer_curtoktype(knit, prs))
#define K_LA_TOKTYPE()      (knitx_lexer_latoktype(knit, prs))

//return true if current/lookahead token matches the token type t
#define K_CURR_MATCHES(t)   (knitx_lexer_curtoktype(knit, prs) == (t))
#define K_LA_MATCHES(t)     (knitx_lexer_latoktype(knit, prs) == (t))
//return a pointer to current token / lookahead token or NULL
#define K_CURR_TOK()        (knitx_lexer_curtok(knit, prs))
#define K_LA_TOK()          (knitx_lexer_latok(knit, prs))

//advance token, or return from current function if there is an error
#define K_ADV_TOK_CRV()          do { \
                                    rv = knitx_lexer_adv(knit, &prs->lex); \
                                    KNIT_CRV(rv); \
                                } while (0)
//advance token, or jmp to label if there is an error
#define K_ADV_TOK_CRV_JMP(label)  do { \
                                      rv = knitx_lexer_adv(knit, &prs->lex); \
                                      KNIT_CRV_JMP(rv, label);\
                                  } while (0)
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
    {KAT_ADD, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  2},
    {KAT_SUB, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  2},
    {KAT_MUL, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  1},
    {KAT_DIV, KAT_IS_BINOP | KAT_IS_LEFT_ASSOC,  1},
    //code assumes minimum is 1
};
#define NOPINFO (sizeof ktokinfo / sizeof ktokinfo[0])
//returns an index to ktokinfo
static inline int knit_tok_info_idx(int toktype) {
    switch (toktype) {
        case KAT_ADD: return 0;break;
        case KAT_SUB: return 1;break;
        case KAT_MUL: return 2;break;
        case KAT_DIV: return 3;break;
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
 * execution plan, and relationship with parsing and lexing
 * suppose we have:
 *    expr = 23 + (8 - 5)
 *              _
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
*/


//box an expr into dynamic memory
//NOTE: this does a shallow copy or a "move". some exprs have resources that they own (ex. str)
static int knitx_save_expr(struct knit *knit, struct knit_prs *prs, struct knit_expr **exprp) {
    void *p;
    int rv = knitx_tmalloc(knit, sizeof **exprp, &p); KNIT_CRV(rv);
    memcpy(p, &prs->expr, sizeof **exprp);
    *exprp = p;
    return KNIT_OK;
}
static int knitx_expr_destroy(struct knit *knit, struct knit_prs *prs, struct knit_expr *expr) {
    knitx_tfree(knit, expr); 
    return KNIT_OK;
}

static int kexpr_save_constant(struct knit *knit, struct knit_prs *prs,  struct knit_expr *expr, int *index_out) {
    struct knit_obj *obj = NULL;
    int rv = KNIT_OK;
    if (expr->exptype == KAX_LITERAL_INT) {
        struct knit_int *intp;
        rv = knitx_int_new(knit, &intp, expr->u.integer); KNIT_CRV(rv);
        obj = ktobj(intp);
    }
    else {
        knit_assert_h(0, "kexpr_save_constant(): unsupported exptype");
    }
    return knitx_block_add_constant(knit, &prs->block, obj, index_out);
}


static int kexpr_expr(struct knit *knit, struct knit_prs *prs, int min_prec); //fwd
static int kexpr_atom(struct knit *knit, struct knit_prs *prs, int min_prec) {
    int rv = KNIT_OK;
    if (K_CURR_MATCHES(KAT_INTLITERAL)) {
        prs->expr.exptype = KAX_LITERAL_INT;
        prs->expr.u.integer = K_CURR_TOK()->data.integer;
        K_ADV_TOK_CRV();
    }
    else if (K_CURR_MATCHES(KAT_OPAREN)) {
        K_ADV_TOK_CRV();
        rv = kexpr_expr(knit, prs, 1); KNIT_CRV(rv);
        if (!K_CURR_MATCHES(KAT_CPAREN)) {
            return knit_error_expected(knit, prs, KAT_CPAREN, "");
        }
        K_ADV_TOK_CRV();
    }
    else if (K_CURR_MATCHES(KAT_VAR)) {
        prs->expr.exptype = KAX_VAR_REF;
        rv = knitx_str_new(knit, &prs->expr.u.str); KNIT_CRV(rv);
        K_ADV_TOK_CRV();
    }
    else if (K_CURR_MATCHES(KAT_STRLITERAL)) {
        prs->expr.exptype = KAX_LITERAL_STR;
        rv = knitx_str_new(knit, &prs->expr.u.str); KNIT_CRV(rv);
        K_ADV_TOK_CRV();
    }
    else {
        return knit_error_expected(knit, prs, KAT_INTLITERAL, "expected an expression");
    }
    return rv;
}

static int knitx_emit_1(struct knit *knit, struct knit_prs *prs, int opcode) {
    knit_assert_h(KINSN_TVALID(opcode), "invalid insn");
    struct knit_insn insn;
    insn.insn_type = opcode;
    insn.op1 = -1;
    insn.op2 = -1;
    int rv = knitx_block_add_insn(knit, &prs->block, &insn); KNIT_CRV(rv); 
    return KNIT_OK;
}
static int knitx_emit_2(struct knit *knit, struct knit_prs *prs, int opcode, int arg1) {
    knit_assert_h(KINSN_TVALID(opcode), "invalid insn");
    struct knit_insn insn;
    insn.insn_type = opcode;
    insn.op1 = arg1;
    insn.op2 = -1;
    int rv = knitx_block_add_insn(knit, &prs->block, &insn); KNIT_CRV(rv); 
    return KNIT_OK;
}
static int knitx_emit_3(struct knit *knit, struct knit_prs *prs, int opcode, int arg1, int arg2) {
    knit_assert_h(KINSN_TVALID(opcode), "invalid insn");
    struct knit_insn insn;
    insn.insn_type = opcode;
    insn.op1 = arg1;
    insn.op2 = arg2;
    int rv = knitx_block_add_insn(knit, &prs->block, &insn); KNIT_CRV(rv); 
    return KNIT_OK;
}


//turn an expr into a sequence of bytecode insns that result in its result being at the top of the stack
static int knitx_emit_expr_eval(struct knit *knit, struct knit_prs *prs, struct knit_expr *expr) {
    knit_assert_h(expr != &prs->expr, "");
    int rv = KNIT_OK;
    if (expr->exptype == KAX_CALL) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LITERAL_STR) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LITERAL_INT) {
        int idx = -1;
        rv = kexpr_save_constant(knit, prs, expr, &idx); /*ml*/ KNIT_CRV(rv);
        rv = knitx_emit_2(knit, prs, KLOAD, idx); /*ml*/ KNIT_CRV(rv);
    }
    else if (expr->exptype == KAX_BIN_OP) {
        rv = knitx_emit_expr_eval(knit, prs, expr->u.bin.lhs); /*ml*/ KNIT_CRV(rv);
        rv = knitx_emit_expr_eval(knit, prs, expr->u.bin.rhs); /*ml*/ KNIT_CRV(rv);
        rv = knitx_emit_1(knit, prs, expr->u.bin.op);
    }
    else if (expr->exptype == KAX_ASSIGNMENT) {
        rv = knitx_emit_expr_eval(knit, prs, 
    }
    else if (expr->exptype == KAX_LITERAL_LIST) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LIST_INDEX) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LIST_SLICE) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_VAR_REF) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_OBJ_DOT) {
        knit_fatal("expr eval currently not implemented");
    }
    else {
        knit_assert_h(0, "");
    }
    return KNIT_OK;
}
static int knitx_emit_ret(struct knit *knit, struct knit_prs *prs, int count) {
    if (count > 1 || count < 0)
        knit_fatal("returning multiple values is not implemented");
    return knitx_emit_2(knit, prs, KRET, count);
}

static int knitx_emit_pop(struct knit *knit, struct knit_prs *prs, int count) {
    return knitx_emit_2(knit, prs, KPOP, count);
}

//turn a binary op into a sequence of bytecode insns that result in its result being at the top of the stack
static int kexpr_binoperation(struct knit *knit, struct knit_prs *prs, int op_token, struct knit_expr *lhs_expr, struct knit_expr *rhs_expr) {
    int insn_type = 0;
    if (op_token == KAT_ADD) {
        insn_type = KADD;
    }
    else if (op_token == KAT_SUB) {
        insn_type = KSUB;
    }
    else if (op_token == KAT_MUL) {
        insn_type = KMUL;
    }
    else if (op_token == KAT_DIV) {
        insn_type = KDIV;
    }
    else if (op_token == KAT_MOD) {
        insn_type = KMOD;
    }
    else {
        knit_assert_h(0, "invalid op type");
    }
    prs->expr.exptype = KAX_BIN_OP;
    prs->expr.u.bin.op = insn_type;
    prs->expr.u.bin.lhs = lhs_expr;
    prs->expr.u.bin.rhs = rhs_expr;
    return KNIT_OK;
}
static int kexpr_expr(struct knit *knit, struct knit_prs *prs, int min_prec) {
    //https://eli.thegreenplace.net/2012/08/02/parsing-expressions-by-precedence-climbing
    int rv = kexpr_atom(knit, prs, min_prec);   KNIT_CRV(rv);

    struct knit_tok *curtok = knitx_lexer_curtok(knit, prs);
    if (!curtok) {
        return knit_error(knit, KNIT_SYNTAX_ERR, "expecting an expression");
    }
    struct knit_expr *lhs_expr = NULL; //we accumulate into this
    while (curtok && knit_tok_is(curtok->toktype, KAT_IS_BINOP) && (knit_tok_prec(curtok->toktype) >= min_prec)) {
        rv = knitx_save_expr(knit, prs, &lhs_expr); /*ml*/ KNIT_CRV(rv);
        int op = curtok->toktype;
        int prec = knit_tok_prec(curtok->toktype);
        int assoc = knit_tok_is(curtok->toktype, KAT_IS_LEFT_ASSOC) ? KAT_IS_LEFT_ASSOC : KAT_IS_RIGHT_ASSOC;
        int next_min_prec = (assoc == KAT_IS_LEFT_ASSOC) ? prec + 1 : prec;
        K_ADV_TOK_CRV(); /*ml*/
        struct knit_expr *rhs_expr = NULL;
        rv = kexpr_expr(knit, prs, next_min_prec); /*ml*/ KNIT_CRV(rv);
        rv = knitx_save_expr(knit, prs, &rhs_expr); /*ml*/ KNIT_CRV(rv);
        rv = kexpr_binoperation(knit, prs, op, lhs_expr, rhs_expr); /*ml*/ KNIT_CRV(rv);
        curtok = knitx_lexer_curtok(knit, prs);
    }
    if (!curtok) {
        knit_clear_error(knit);
    }
    //at return, the resulting experssion will be at .prs.expr
    return rv;
}

static int knitx_expr(struct knit *knit, struct knit_prs *prs) {
    int rv = kexpr_expr(knit, prs, 1); /*ml*/ KNIT_CRV(rv);
    return rv;
}
//boolean return value
static int knitx_is_lvalue(struct knit *knit, struct knit_expr *exp) {
    //TODO: there are other kinds of lvalues
    return exp->exptype == KAX_VAR_REF;
}
//lhs is expected at prs.expr
static int knitx_assignment_stmt(struct knit *knit, struct knit_prs *prs) {
    struct knit_expr *lhs_expr = NULL;
    if (!knitx_is_lvalue(knit, &prs->expr)) {
        return knit_error(knit, KNIT_SYNTAX_ERR, "attempting to assign to a non-lvalue");
    }
    int rv = knitx_save_expr(knit, prs, &lhs_expr); /*ml*/ KNIT_CRV(rv);
    rv = knitx_expr(knit, prs); /*ml*/ KNIT_CRV(rv);
    struct knit_expr *rhs_expr = NULL;
    rv = knitx_save_expr(knit, prs, &rhs_expr); /*ml*/ KNIT_CRV(rv);
    prs->expr.exptype = KAX_ASSIGNMENT;
    prs->expr.u.bin.lhs = lhs_expr;
    prs->expr.u.bin.rhs = rhs_expr;
    return KNIT_OK;
}
static int knitx_stmt(struct knit *knit, struct knit_prs *prs) {
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
    if (K_CURR_MATCHES(KAT_INTLITERAL)      ||
             K_CURR_MATCHES(KAT_STRLITERAL) ||
             K_CURR_MATCHES(KAT_VAR)        ||
             K_CURR_MATCHES(KAT_OPAREN)     ||
             K_CURR_MATCHES(KAT_OBRACKET)   ||
             K_CURR_MATCHES(KAT_OPAREN)       ) 
    {
        //expr
        rv = knitx_expr(knit, prs);
        if (rv != KNIT_OK)
            return rv;
        if (K_CURR_MATCHES(KAT_EQUALS)) {
            K_ADV_TOK_CRV();
            rv = knitx_assignment_stmt(knit, prs);
        }
        else {
            struct knit_expr *root_expr = NULL;
            rv = knitx_save_expr(knit, prs, &root_expr); /*ml*/ KNIT_CRV(rv);
            rv = knitx_emit_expr_eval(knit, prs,  root_expr);
            knitx_emit_pop(knit, prs, 1); //assumes only 1 result pushed
            knitx_expr_destroy(knit, prs, root_expr);
        }
        if (!K_CURR_MATCHES(KAT_SEMICOLON)) {
            return knit_error_expected(knit, prs, KAT_SEMICOLON, "");
        }
        K_ADV_TOK_CRV(); //skip ';'
    }
    else {
        return knit_error_expected(knit, prs, KAT_INTLITERAL, "");
    }
    return KNIT_OK;
}
static int knitx_prog(struct knit *knit, struct knit_prs *prs) {
    int rv = KNIT_OK;
    while (!K_CURR_MATCHES(KAT_EOF) && rv == KNIT_OK) {
        rv = knitx_stmt(knit, prs); 
    }
    KNIT_CRV(rv);
    rv = knitx_emit_ret(knit, prs, 0);
    return rv;
}
#undef K_LA_MATCHES
#undef K_CURR_MATCHES
/*END OF PARSING FUNCS*/
static int knitx_eval(struct knit *knit, struct knit_expr *expr) {
    return KNIT_OK;
}

//never returns null
const char *knitx_obj_type_name(struct knit *knit, struct knit_obj *obj) {
    if (obj->u.ktype == KNIT_INT)      return "KNIT_INT";
    else if (obj->u.ktype == KNIT_STR) return "KNIT_STR";
    else if (obj->u.ktype == KNIT_LIST) return "KNIT_LIST";
    return "ERR_UNKNOWN_TYPE";
}


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
        int rv = knitx_int_new(knit, &ri, 0);
        *r = ktobj(ri); KNIT_CRV(rv);
        switch (op) {
            case KADD: ri->value = ai->value + bi->value; break;
            case KSUB: ri->value = ai->value - bi->value; break;
            case KMUL: ri->value = ai->value * bi->value; break;
            case KDIV: ri->value = ai->value / bi->value; break;
            case KMOD: ri->value = ai->value % bi->value; break;
            default:
                knit_fatal("unsupported op for ints: %s", knit_insn_name(op));
        }
    }
    else {
        knit_fatal("knitx_op_add(): unsupported types for %s: %s and %s", knit_insn_name(op), knitx_obj_type_name(knit, a), knitx_obj_type_name(knit, b));
    }
    return KNIT_OK;
}

static int knitx_op_exec_binop(struct knit *knit, struct knit_stack *stack, int op) {
    knit_assert_s(stack->vals.len >= 2, "");
    struct knit_obj *a = stack->vals.data[stack->vals.len - 2];
    struct knit_obj *b = stack->vals.data[stack->vals.len - 1];
    struct knit_obj *r;
    int rv = knitx_op_do_binop(knit, a, b, &r, op);
    knitx_obj_decref(knit, a);
    knitx_obj_decref(knit, b);
    knitx_stack_assign_range_null(knit, stack, stack->vals.len - 1, stack->vals.len);
    knit_assert_h((rv == KNIT_OK && r) || (rv != KNIT_OK && !r), "");
    knitx_stack_assign_o(knit, stack, stack->vals.len - 2, ktobj(r));
    stack->vals.len--; //pop1
    return KNIT_OK;
}

static int knitx_exec(struct knit *knit) {
    struct knit_stack *stack = &knit->ex.stack;
    struct knit_frame_darr *frames = &knit->ex.stack.frames;
    struct knit_objp_darr *vals = &knit->ex.stack.vals;

    knit_assert_h(frames->len > 0, "");
    struct knit_frame *top_frm = &frames->data[frames->len-1];
    struct knit_block *block = top_frm->block;
    knit_assert_h(top_frm->bsp >= 0 && top_frm->bsp <= vals->len, "");

    int rv = KNIT_OK;
    while (1) {
        knit_assert_s(top_frm->ip < block->insns.len, "executing out of range instruction");
        struct knit_insn *insn = &block->insns.data[top_frm->ip];
        int t = vals->len; //values stack size, top of stack is vals->data[t-1]
        int op = insn->insn_type;
        rv = KNIT_OK;
        if (op == KPUSH) { /*inputs: (index,)                       op: s[t] = s[index]; t++;*/
            knit_fatal("insn not supported: %s", knit_insn_name(op));
        }
        else if (op == KPOP) {
            knit_assert_s(insn->op1 > 0 && insn->op1 <= vals->len, "popping too many values");
            for (int i=vals->len - insn->op1; i < vals->len; i++) {
                kdecref(vals->data[i]);
            }
            rv = knitx_stack_rpop(knit, stack, insn->op1);
        }
        else if (op == KLOAD) {
            knit_assert_s(insn->op1 >= 0 && insn->op1 < block->constants.len, "loading out of range constant");
            rv = knitx_stack_rpush(knit, stack, block->constants.data[insn->op1]);
        }
        else if (op == KCALL) {
            knit_fatal("insn not supported: %s", knit_insn_name(op));
        }
        else if (op == KINDX) {
            knit_fatal("insn not supported: %s", knit_insn_name(op));
        }
        else if (op == KRET) {
            knit_assert_h((vals->len - top_frm->bsp) >= insn->op1, "returning too many values");
            /*
             * [locals and tmps] : deref
             * [return values] : move up
             */
            for (int i=top_frm->bsp; i<(vals->len - insn->op1); i++) {
                kdecref(vals->data[i]);
                knitx_stack_assign_null(knit, stack, i);
            }
            for (int i=0; i<insn->op1; i++) {
                int from_idx = vals->len    - i - 1;
                int to_idx   = top_frm->bsp + i;
                vals->data[to_idx] = vals->data[from_idx];
                if (from_idx > top_frm->bsp + insn->op1)
                    knitx_stack_assign_null(knit, stack, from_idx);
            }
            vals->len = top_frm->bsp + insn->op1; //pops all unneeded
            rv = knitx_stack_pop_frame(knit, stack);
            if (rv != KNIT_OK)
                return rv;
            if (frames->len == 0) {
                goto done;
            }
            top_frm = &frames->data[frames->len-1];
            block = top_frm->block;


            knit_assert_h(top_frm->bsp >= 0 && top_frm->bsp <= vals->len, "");
        }
        else if (op == KADD || op == KSUB || op == KMUL || op == KDIV || op == KMOD) {
            rv = knitx_op_exec_binop(knit, stack, op);
        }
        else {
            knit_fatal("insn not supported: %s", knit_insn_name(op));
        }
        if (rv != KNIT_OK) {
            //TODO handle error by returning from function
            return rv;
        }
        top_frm->ip++; //we have no support for branches currently
    }
done:
    return KNIT_OK;
}
static int knitx_block_exec(struct knit *knit, struct knit_block *block) {
    int rv = knitx_stack_push_frame_for_call(knit, block); KNIT_CRV(rv);
    rv = knitx_exec(knit); KNIT_CRV(rv);
    return KNIT_OK;
}

static int knitx_exec_str(struct knit *knit, const char *program) {
    struct knit_prs prs;
    for (int i=0; i<2; i++) {
        int rv;
        knitx_prs_init(knit, &prs);
        knitx_lexer_init_str(knit, &prs.lex, program);
        if (i == 0) {
            knitx_lexdump(knit, &prs.lex);
        }
        else if (i == 1) {
            knitx_prog(knit, &prs);
            knitx_block_dump(knit, &prs.block);
            knitx_block_exec(knit, &prs.block);
            knitx_stack_dump(knit, &knit->ex.stack, -1, -1);
        }
        knitx_lexer_deinit(knit, &prs.lex);
        knitx_prs_deinit(knit, &prs);
    }

    return KNIT_OK; //dummy
}

static int knitx_init(struct knit *knit, int opts) {
    int rv;
    if (opts & KNIT_POLICY_CONTINUE)
        knit_set_error_policy(knit, KNIT_POLICY_CONTINUE);
    else 
        knit_set_error_policy(knit, KNIT_POLICY_EXIT);

    knit->err_msg = NULL;
    knit->err = KNIT_OK;

    rv = mem_hasht_init(&knit->mem_ht, 256);

    if (rv != MEM_HASHT_OK) {
        rv = knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize mem hashtable");
        goto end;
    }
    rv = vars_hasht_init(&knit->vars_ht, 32);
    if (rv != VARS_HASHT_OK) {
        rv = knit_error(knit, KNIT_RUNTIME_ERR, "couldn't initialize vars hashtable");
        goto cleanup_mem_ht;
    }

    rv = knitx_exec_state_init(knit, &knit->ex);
    if (rv != KNIT_OK)
        goto cleanup_vars_ht;

    return KNIT_OK;

cleanup_vars_ht:
    vars_hasht_deinit(&knit->vars_ht);
cleanup_mem_ht:
    mem_hasht_deinit(&knit->mem_ht);
end:
    return rv;
}
static int knitx_deinit(struct knit *knit) {
    struct mem_hasht_iter iter;
    int rv = mem_hasht_begin_iterator(&knit->mem_ht, &iter);
    knit_assert_h(rv == MEM_HASHT_OK, "knitx_deinit(): couldnt iterate memory set");
    for (; mem_hasht_iter_check(&iter);  mem_hasht_iter_next(&knit->mem_ht, &iter)) {
        void *p = iter.pair->key;
        knitx_rfree(knit, p);
    }
    mem_hasht_deinit(&knit->mem_ht);
    vars_hasht_deinit(&knit->vars_ht);
    return KNIT_OK;
}
