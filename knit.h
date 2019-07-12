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
    abort();
}
static void knit_assert_s(int condition, const char *fmt, ...) {
    (void) fmt;
    assert(condition);
}
static void knit_assert_h(int condition, const char *fmt, ...) {
    (void) fmt;
    assert(condition);
}

static struct knit_str *knit_as_str(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_STR, "knit_as_str(): invalid argument type, expected string");
    return &obj->u.str;
}
static struct knit_list *knit_as_list(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_LIST, "knit_as_list(): invalid argument type, expected list");
    return &obj->u.list;
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
static int knitx_block_dump(struct knit *knit, struct knit_block *block) { 
    for (int i=0; i<block->insns.len; i++) {
        struct knit_insn *insn = &block->insns.data[i];
        struct knit_insninfo *inf = &knit_insninfo[insn->insn_type];
        printf("%d\t\t%s\n", i, inf->rep);
    }
    return KNIT_OK;
}

#define kincref(p) (p)
#define kdecref(p) (p)

/*EXECUTION STATE FUNCS*/
static int knitx_frame_init(struct knit *knit, struct knit_frame *frame, struct knit_block *block, int ip) {
    frame->block = kincref(block);
    frame->ip = ip;
    return KNIT_OK;
}
static int knitx_frame_deinit(struct knit *knit, struct knit_frame *frame) {
    kdecref(frame->block);
    return KNIT_OK;
}
static int knitx_stack_init(struct knit *knit, struct knit_stack *stack) {
    int rv = knit_frame_darr_init(&stack->frames, 128);
    if (rv != KNIT_FRAME_DARR_OK) {
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_init(): initializing darray failed");
    }
    return KNIT_OK;
}
static int knitx_stack_deinit(struct knit *knit, struct knit_stack *stack) {
    return knit_frame_darr_deinit(&stack->frames);
}
static int knitx_stack_push_frame_0(struct knit *knit, struct knit_stack *stack, struct knit_block *block) {
    struct knit_frame frm;
    int rv = knitx_frame_init(knit, &frm, block, 0);
    if (rv != KNIT_OK) {
        return rv;
    }
    rv = knit_frame_darr_push(&stack->frames, &frm);
    if (rv != KNIT_FRAME_DARR_OK) {
        knitx_frame_deinit(knit, &frm);
        return knit_error(knit, KNIT_RUNTIME_ERR, "knit_stack_push_frame_0(): pushing a stack frame failed");
    }
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
    lxr->offset++; //skip '
    return knitx_lexer_add_tok(knit, lxr, KAT_STRLITERAL, beg, lxr->offset - beg, lxr->lineno, lxr->colno, NULL);
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

//str = token
static int knitx_knit_tok_set_str(struct knit *knit, struct knit_lex *lxr, struct knit_tok *tok, struct knit_str *str) {
    knit_assert_h(!!tok && !!str, "");
    knit_assert_h(tok->offset < lxr->input->len, "");
    int rv = knitx_str_strlcpy(knit, str, lxr->input->str + tok->offset, tok->len);
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
        printf("input: '''\n%s\n'''\n", lxr->input->str);
    struct knit_tok *tok;
    struct knit_str *tokstr;
    int rv = knitx_str_new(knit, &tokstr);
    if (rv != KNIT_OK)
        return rv;
    while ((rv = knitx_lexer_peek_cur(knit, lxr, &tok)) == KNIT_OK) {
        if (tok->toktype == KAT_EOF)
            break;
        knitx_knit_tok_repr_set_str(knit, lxr, tok, tokstr);
        printf("tok %d: %s\n", lxr->tokno, tokstr->str);
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
//box an expr into dynamic memory
//NOTE: this does a shallow copy or a "move". some exprs have resources that they own (ex. str)
static int knitx_save_expr(struct knit *knit, struct knit_prs *prs, struct knit_expr **exprp) {
    void *p;
    int rv = knitx_tmalloc(knit, sizeof **exprp, &p); KNIT_CRV(rv);
    memcpy(p, &prs->expr, sizeof **exprp);
    *exprp = p;
    return KNIT_OK;
}

static int kexpr_save_constant(struct knit *knit, struct knit_prs *prs, int *index_out) {
    struct knit_expr *expr = &prs->expr; //this is supposed to be filled with the value to be saved
    struct knit_obj *obj = NULL;
    int rv = KNIT_OK;
    if (expr->exptype == KAX_LITERAL_INT) {
        struct knit_int *intp;
        rv = knitx_int_new(knit, &intp, expr->u.integer); KNIT_CRV(rv);
        obj = (struct knit_obj *) intp;
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
    else {
        return knit_error_expected(knit, prs, KAT_INTLITERAL, "expected an expression");
    }
    return rv;
}

struct knit_insn knit_make_insn(int insn_type) {
    struct knit_insn insn;
    insn.insn_type = insn_type;
    return insn;
}
//turn an expr into a sequence of bytecode insns that result in its result being at the top of the stack
static int kexpr_eval(struct knit *knit, struct knit_expr *expr) {
    if (expr->exptype == KAX_CALL) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LITERAL_STR) {
        knit_fatal("expr eval currently not implemented");
    }
    else if (expr->exptype == KAX_LITERAL_INT) {
        knit_fatal("expr eval currently not implemented");
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

static int kemit_expr_eval_push(struct knit *knit, struct knit_prs *prs)
{
    knit_fatal("not implemented");
    return KNIT_OK;
}
static int kemit_pop(struct knit *knit, struct knit_prs *prs) {
    knit_fatal("not implemented");
    return KNIT_OK;
}

//turn a binary op into a sequence of bytecode insns that result in its result being at the top of the stack
static int kexpr_binoperation(struct knit *knit, struct knit_prs *prs, int op_token, struct knit_expr *lhs_expr, struct knit_expr *rhs_expr) {
    const char *trep = knit_knit_tok_name(op_token);
    printf("op is: %s\n", trep ? trep : "???");
    
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
        insn_type = KDIVM;
    }
    else {
        knit_assert_h(0, "invalid op type");
    }
    struct knit_insn insn = knit_make_insn(insn_type);
    int rv = knitx_block_add_insn(knit, &prs->block, &insn); KNIT_CRV(rv); 

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
    return kexpr_expr(knit, prs, 1);
}
static int knitx_prog(struct knit *knit, struct knit_prs *prs) {
    /*
    symbol          first
    program         Name + '(' 
    stat            Name + '(' 
    var             Name + '('
    prefixexp       Name + '('
    exp             null + false + Numeral + LiteralString + '[' + '(' + Name + f(unop)
    literallist     '['
    */
    if (K_CURR_MATCHES(KAT_INTLITERAL) || K_CURR_MATCHES(KAT_STRLITERAL)) {
        knitx_expr(knit, prs);
    }
    else {
        return knit_error_expected(knit, prs, KAT_INTLITERAL, "");
    }
    return KNIT_OK;
}
#undef K_LA_MATCHES
#undef K_CURR_MATCHES
/*END OF PARSING FUNCS*/
static int knitx_eval(struct knit *knit, struct knit_expr *expr) {
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
        }
        knitx_lexer_deinit(knit, &prs.lex);
        knitx_prs_deinit(knit, &prs);
    }

    return KNIT_OK; //dummy
}
