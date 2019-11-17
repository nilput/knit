#ifndef KNIT_COMMON_H
#define KNIT_COMMON_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "kdata.h"

//enable checks that are only there for testing
#define KNIT_CHECKS

//#define KNIT_DEBUG_PRINT for debug output (lexer tokens, code, etc..)

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


//fwd
static int knit_error(struct knit *knit, int err_type, const char *fmt, ...);
static const char *knitx_obj_type_name(struct knit *knit, struct knit_obj *obj);
static int knitx_obj_rep(struct knit *knit, struct knit_obj *obj, struct knit_str *outi_str, int human);
static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj);
static int knit_error(struct knit *knit, int err_type, const char *fmt, ...);
static int knitx_register_new_ptr_(struct knit *knit, void *m);
static int knitx_unregister_ptr_(struct knit *knit, void *m);
static int knitx_update_ptr_(struct knit *knit, void *old_m, void *new_m);
static int knitx_str_set_cap(struct knit *knit, struct knit_str *str, int capacity);
static int knit_strl_eq(const char *a, const char *b, size_t len);
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


static void knitx_obj_incref(struct knit *knit, struct knit_obj *obj) {
}

static void knitx_obj_decref(struct knit *knit, struct knit_obj *obj) {
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


/*currently not used in a meaningful way, ARC might be used or garbage collection*/
#define kincref(p) knitx_obj_incref(knit, (struct knit_obj *)(p))
#define kdecref(p) knitx_obj_decref(knit, (struct knit_obj *)(p))
#define ktobj(p) ((struct knit_obj *) (p))
#define kunreachable()



#endif //#ifndef KNIT_COMMON_H