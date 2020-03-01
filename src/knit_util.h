#ifndef KNIT_UTIL_H
#define KNIT_UTIL_H
#include "kdata.h"
#include "knit_mem_stats.h"
static void knit_assert_h(int condition, const char *fmt, ...);
static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj); //fwd
static int knit_error(struct knit *knit, int err_type, const char *fmt, ...);

#ifdef KNIT_MEM_STATS
#define KNIT_MAX_ALIGNMENT_REQ 8
#define KNIT_WRAPPED_DATA_SZ  sizeof(size_t)
#define KNIT_WRAPPED_SZ (KNIT_WRAPPED_DATA_SZ + (KNIT_WRAPPED_DATA_SZ % KNIT_MAX_ALIGNMENT_REQ))

static size_t ptr_wrap_needed_sz(size_t sz) {
    return sz + KNIT_WRAPPED_SZ;
}
static void ptr_wrap_szt_store(void *up, size_t n) {
    unsigned char *c = up;
    for (unsigned i=0; i<sizeof(size_t); i++) {
        c[i] = n & 0xFF;
        n >>= 8;
    } 
}
static size_t ptr_wrap_szt_get(void *up) {
    unsigned char *c = up;
    size_t n = 0;
    for (int i=sizeof(size_t) - 1; i >= 0; i--) {
        n <<= 8;
        n |= c[i];
    } 
    return n;
}
static void *ptr_wrap(void *p, size_t sz) {
    ptr_wrap_szt_store(p, sz);   
    return ((char *) p) + KNIT_WRAPPED_SZ;
}
static void *ptr_unwrap(void *p) {
    if (!p)
        return NULL;
    return ((char *) p) - KNIT_WRAPPED_SZ;
}
static size_t ptr_wrap_get_sz(void *p) {
    if (!p) {
        return 0;
    }
    return ptr_wrap_szt_get(ptr_unwrap(p));
}
#else
static size_t ptr_wrap_needed_sz(size_t sz) {
    return sz;
}
static void *ptr_wrap(void *p, size_t sz) {
    return p;
}
static void *ptr_unwrap(void *p) {
    return p;
}
static size_t ptr_wrap_get_sz(void *p) {
    return 0;
}
#endif



static int knitx_rfree(struct knit *knit, void *p) {
    (void) knit;
    KMEMSTAT_FREE(knit, ptr_wrap_get_sz(p));
    free(ptr_unwrap(p));
    return KNIT_OK;
}
static int knitx_rmalloc(struct knit *knit, size_t sz, void **m) {
    KMEMSTAT_ALLOC(knit, sz);
    knit_assert_h(sz, "knit_malloc(): 0 size passed");
    void *p = malloc(ptr_wrap_needed_sz(sz));
    *m = NULL;
    if (!p)
        return knit_error(knit, KNIT_NOMEM, "knitx_malloc(): malloc() returned NULL");
    *m = ptr_wrap(p, sz);
    return KNIT_OK;
}
static int knitx_rrealloc(struct knit *knit, void *p, size_t sz, void **m) {
    KMEMSTAT_REALLOC(knit, ptr_wrap_get_sz(p), sz);
    if (!sz) {
        int rv = knitx_rfree(knit, p);
        *m = NULL;
        return rv;
    }
    void *np = realloc(ptr_unwrap(p), ptr_wrap_needed_sz(sz));
    if (!np) {
        return knit_error(knit, KNIT_NOMEM, "knitx_realloc(): realloc() returned NULL");
    }
    *m = ptr_wrap(np, sz);
    return KNIT_OK;
}
static int knitx_tmalloc(struct knit *knit, size_t sz, void **m) {
    return knitx_rmalloc(knit, sz, m);
}
static int knitx_tfree(struct knit *knit, void *p) {
    return knitx_rfree(knit, p);
}
static int knitx_trealloc(struct knit *knit, void *p, size_t sz, void **m) {
    return knitx_rrealloc(knit, p, sz, m);
}

#endif
