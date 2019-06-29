/*
Copyright 2019 Turki Alsaleem

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*
needed functions that should be defined before including this: 
    int mem_hasht_key_eq_cmp(mem_hasht_key_type *key_1, mem_hasht_key_type *key_2) (returns 0 if equal)
    size_t mem_hasht_hash(mem_hasht_key_type *key)
needed typedefs: 
     typedef <type> mem_hasht_key_type; 
     typedef <type> mem_hasht_value_type; 

    as an example: key_type can be a string (char *)
                   value_type can be a struct foo; for example
    in this example, the function mem_hasht_key_cmp would recieve: char **, char **
                     the function mem_hasht would recieve struct foo *
                     the same is true for the api's functions, insert() expects (key: char **, value: struct foo *)
                                                               remove() would expect (key: char **)
*/


//the following is an anti-include-guard

#ifdef MEM_HASHT_H
#error "the header can only be safely included once"
#endif // #ifdef MEM_HASHT_H
#define MEM_HASHT_H

#include <stdlib.h> //malloc, free
#include <stdbool.h> 
#include <string.h> //memset, memcpy, ...
#include "div_32_funcs.h" //divison functions


static int mem_hasht_get_adiv_power_idx(size_t at_least) {
    //starts at 2
    for (int i=2; i < (int)adiv_n_values; i++) {
        if (adiv_values[i] >= at_least) 
            return i;
    }
    return -1; //error, must be handled
}

//#define MEM_HASHT_DBG

#define MEM_HASHT_MIN_TABLESIZE 4

#ifdef MEM_HASHT_DBG
    #include <assert.h>
    #define MEM_HASHT_DBG_CODE(...) do { __VA_ARGS__ } while(0)
    #define MEM_HASHT_ASSERT(cond, msg) assert(cond)
#else
    #define MEM_HASHT_DBG_CODE(...) 
    #define MEM_HASHT_ASSERT(cond, msg) 
#endif

//notice how logic is inverted? so we can memset with 0
#define MEM_HASHT_VLT_IS_NOT_EMPTY      (1U << 1)
#define MEM_HASHT_VLT_IS_DELETED        (1U << 2)
#define MEM_HASHT_VLT_IS_CORRUPT        (1U << 3)
//long is used for all lengths / sizes


struct mem_hasht_pair_type {
    //bits:
    //[0...8]  flags
    //[8..32]  partial hash
    unsigned int pair_data; //this is two parts: the flags, and the partial hash
    mem_hasht_key_type   key;
    mem_hasht_value_type value;
};

//pair type functions
static unsigned char mem_hasht_pr_flags(struct mem_hasht_pair_type *prt) {
    return prt->pair_data & 0xFF;
}
static unsigned int mem_hasht_pr_get_partial_hash(struct mem_hasht_pair_type *prt) {
    const unsigned int upper_24bits = 0xFFFFFF00;
    return prt->pair_data & upper_24bits;
}
//partial hashes have the lower 8 bits equal to zero, (there we store flags)
//this is assuming unsigned int is at least 32 bits, 
//the alternative would be uint32_t stuff
static unsigned int mem_hasht_hash_to_partial_hash(size_t full_hash) {
    const unsigned int lower_24bits = 0x00FFFFFF;
    return (full_hash & lower_24bits) << 8;
}
static unsigned int mem_hasht_pr_combine_flags_and_partial_hash(unsigned char flags, unsigned int partial_hash) {
    MEM_HASHT_ASSERT((partial_hash & 0xFF) == 0, "invalid partial hash");
    return partial_hash | flags;
}
static void mem_hasht_pr_set_flags(struct mem_hasht_pair_type *prt, unsigned char flags) {
    prt->pair_data = mem_hasht_pr_combine_flags_and_partial_hash(flags, mem_hasht_pr_get_partial_hash(prt));
}


//[is_deleted] [is_not_empty]
//     0            0          empty
//     0            1          occupied
//     1            0          invalid state
//     1            1          deleted

static bool mem_hasht_pr_is_empty(struct mem_hasht_pair_type *prt) {
    return (! (mem_hasht_pr_flags(prt) & MEM_HASHT_VLT_IS_NOT_EMPTY)); //false mean occupied or deleted
}
static bool mem_hasht_pr_is_corrupt(struct mem_hasht_pair_type *prt) {
    const unsigned char deleted_and_empty_mask = (MEM_HASHT_VLT_IS_DELETED | MEM_HASHT_VLT_IS_NOT_EMPTY); 
    const unsigned char deleted_and_empty      = (MEM_HASHT_VLT_IS_DELETED | 0); //invalid state
    return   (mem_hasht_pr_flags(prt) & MEM_HASHT_VLT_IS_CORRUPT) || 
             ((mem_hasht_pr_flags(prt) & deleted_and_empty_mask) == deleted_and_empty);
}
static bool mem_hasht_pr_is_deleted(struct mem_hasht_pair_type *prt) {
    return   (mem_hasht_pr_flags(prt) & MEM_HASHT_VLT_IS_DELETED); //false means occupied or empty
}
static bool mem_hasht_pr_is_occupied(struct mem_hasht_pair_type *prt) {
    //occupied here means an active bucket that contains a value
    MEM_HASHT_ASSERT(!mem_hasht_pr_is_corrupt(prt), "corrupt element found");
    return !mem_hasht_pr_is_empty(prt) && !mem_hasht_pr_is_deleted(prt); 
}

typedef void * (*mem_hasht_malloc_fptr)(size_t sz, void *userdata);
typedef void * (*mem_hasht_realloc_fptr)(void *ptr, size_t sz, void *userdata);
typedef void (*mem_hasht_free_fptr)(void *ptr, void *userdata);

struct mem_hasht_alloc_funcs {
    mem_hasht_malloc_fptr alloc;
    mem_hasht_realloc_fptr realloc;
    mem_hasht_free_fptr free;
    void *userdata;
};
static void *mem_hasht_def_malloc(size_t sz, void *unused_userdata_) {
    (void) unused_userdata_;
    return malloc(sz); 
}
static void *mem_hasht_def_realloc(void *ptr, size_t sz, void *unused_userdata_) {
    (void) unused_userdata_;
    return realloc(ptr, sz); 
}
static void  mem_hasht_def_free(void *ptr, void *unused_userdata_) {
    (void) unused_userdata_;
    free(ptr); 
}

enum MEM_HASHT_ERR {
    MEM_HASHT_OK,
    MEM_HASHT_ALLOC_ERR,
    MEM_HASHT_INVALID_REQ_SZ,
    MEM_HASHT_FAILED_AT_RESIZE,

    MEM_HASHT_NOT_FOUND = -1, 
    MEM_HASHT_DUPLICATE_KEY = -2,
    //the table refused to change the size, becuase it thinks it's smart and there's no need, this can be forced though (WIP)
    MEM_HASHT_RESIZE_REFUSE = -3, 
    MEM_HASHT_ITER_STOP = -4,
    MEM_HASHT_ITER_FIRST = -5,

    MEM_HASHT_INVALID_TABLE_STATE = -6, //non recoverable, the only safe operation to do is to call deinit
};

struct mem_hasht {
    struct mem_hasht_pair_type *tab;
    adiv_fptr div_func; //a pointer to a function that does fast division

    long nelements; //number of active buckets (ones that are not empty, and not deleted)
    long ndeleted;
    long nbuckets;
    long nbuckets_po2; //power of two
    long grow_at_gt_n;  //saved result of computation
    long shrink_at_lt_n; 
    long grow_at_percentage; // (divide by 100, for example 0.50 is 50)
    long shrink_at_percentage; 
    struct mem_hasht_alloc_funcs memfuncs;
};


//shrink at, grow at are percentages [0, 99] inclusive, they must fulfil (grow_at / shrink_at) > 2.0
//the function can fail
static int mem_hasht_init_parameters(struct mem_hasht *ht, long shrink_at, long grow_at) {
    bool stupid_value = (shrink_at > 99 || shrink_at < 0 || grow_at > 99 || grow_at < 0);
    if (stupid_value || (shrink_at*2 >= grow_at)) {
        //this fails when, (grow_at / shrink_at) <= 2.0
        //for example, suppose you have shrink_at = 20, grow_at = 35, nelements = 19, nbuckets = 100
        //when we shrink, we shrink by dividing by two, so then we have:
        //nelements = 19, nbuckets = 50
        //now 19/50 is 38%, which means we need to grow again!, therfore grow_at must be greater than shrink_at * 2, preferably way greater
        //a sane value is like 2 to 6, or 2 to 8
        //good values: (20, 60), (15, 45), (15, 50), (15, 60)
        //bad values: (20, 30), (30, 50) etc..
        return MEM_HASHT_INVALID_REQ_SZ;
    }
    ht->grow_at_percentage   = grow_at;
    ht->shrink_at_percentage = shrink_at;

    return MEM_HASHT_OK;
}

//see notes above in mem_hasht_init_parameters(), they apply to this function
static int mem_hasht_set_parameters(struct mem_hasht *ht, long shrink_at, long grow_at) {
    int rv = mem_hasht_init_parameters(ht, shrink_at, grow_at);
    if (rv != MEM_HASHT_OK)
        return rv;
    //this can potentially overflow, maybe we should cast to size_t
    ht->grow_at_gt_n = (ht->nbuckets * ht->grow_at_percentage) / 100;
    ht->shrink_at_lt_n = (ht->nbuckets * ht->shrink_at_percentage) / 100;
    return rv;
}

static long mem_hasht_calc_nelements_to_nbuckets(long needed_nelements, long shrink_at_percentage, long grow_at_percentage) {
    //let ratio = needed_nelements / x 
    //we want an x that fullfills: 
    //ratio > shrink_percent AND ratio < grow_percent
    //pick any ratio between the two: r1 = (shrink_percent + grow_percent) / 2
    //x would be = needed_elements / r1
    //
    //since we store percentages as ints implictly a fraction of 100, r1 = some_integer / 100
    //x = needed_nelements * (100 / some_integer)
    //x = needed_nelements * 100 / some_integer
    long r1i = (shrink_at_percentage + grow_at_percentage) / 2;
    r1i = r1i <= 0 ? 1 : r1i;
    long needed_nbuckets_opt_big = (needed_nelements * 100) / r1i;
    r1i = grow_at_percentage - (grow_at_percentage - shrink_at_percentage) / 3;
    long needed_nbuckets_opt_small = needed_nbuckets_opt_big;
    if (r1i > 0) 
        needed_nbuckets_opt_small = (needed_nelements * 100) / r1i;

    long needed_nbuckets = needed_nbuckets_opt_big;

    if (needed_nbuckets > MEM_HASHT_MIN_TABLESIZE) {
#ifdef MEM_HASHT_DBG
        long ratio_test       = ((needed_nelements * 100) / needed_nbuckets);
        MEM_HASHT_ASSERT( ratio_test >= shrink_at_percentage, "ratio calculation failed");
        MEM_HASHT_ASSERT( ratio_test <= grow_at_percentage, "ratio calculation failed");
#endif
        long ratio_test_small = ((needed_nelements * 100) / needed_nbuckets_opt_small);
        if (ratio_test_small > shrink_at_percentage)
            needed_nbuckets = needed_nbuckets_opt_small;
    }
    else {
        //clamp to the minimum
        needed_nbuckets = MEM_HASHT_MIN_TABLESIZE;
    }
    MEM_HASHT_ASSERT(needed_nbuckets > needed_nelements, "ratio calculation failed");
    return needed_nbuckets;
}

//this is not very trivial, multiple fields are tied to each other and they need to be consistent
//if this fails it doesnt change the field
static int mem_hasht_change_sz_field(struct mem_hasht *ht, long nbuckets, bool force) {
    (void) force;
    //ignore values that are too small
    nbuckets = nbuckets < MEM_HASHT_MIN_TABLESIZE ? MEM_HASHT_MIN_TABLESIZE : nbuckets;
    long nbuckets_po2 = mem_hasht_get_adiv_power_idx(nbuckets);
    if (nbuckets_po2 < 0)
        return MEM_HASHT_INVALID_REQ_SZ; //too big
    MEM_HASHT_ASSERT(nbuckets_po2 >= 2 && nbuckets_po2 < adiv_n_values, "adiv failed");
    long nbuckets_prime = adiv_values[nbuckets_po2];
    ht->nbuckets     = nbuckets_prime;
    ht->nbuckets_po2 = nbuckets_po2;
    ht->div_func = adiv_funcs[ht->nbuckets_po2];
    MEM_HASHT_ASSERT(ht->div_func, "invalid adiv fptr");

#ifdef MEM_HASHT_DBG
    long rnd = rand();
    MEM_HASHT_ASSERT(ht->div_func(rnd) == rnd % ht->nbuckets, "adiv failed");
#endif// MEM_HASHT_DBG
    
    MEM_HASHT_ASSERT(ht->shrink_at_percentage > 0 && ht->grow_at_percentage > ht->shrink_at_percentage, "invalid growth parameters");
    //update cached result of division
    mem_hasht_set_parameters(ht, ht->shrink_at_percentage, ht->grow_at_percentage);
    return MEM_HASHT_OK;
}
static void mem_hasht_zero_sz_field(struct mem_hasht *ht) {
    ht->nbuckets = 0;
    ht->nbuckets_po2 = 0;
    ht->div_func = NULL;
}

//tries to find memory corruption
//flags are flags to consider a failure
//for the three query* ints, 0 means i dont care, 1 expect it to be, -1 means expect it to be not
//for example mem_hasht_dbg_check(ht, 0, ht->nbuckets, 0, 0, -1) -> fail if any are corrupt
static bool mem_hasht_dbg_check(struct mem_hasht *ht, long beg_idx, long end_idx, int query_empty, int query_deleted, int query_corrupt) {
    long len = end_idx - beg_idx;
    for (int i=0; i<len; i++) {
        struct mem_hasht_pair_type *pair = ht->tab + i + beg_idx;
        int expect[3] = {
            query_empty,
            query_deleted,
            query_corrupt, 
        };
        int found[3] = { 
            mem_hasht_pr_is_empty(pair),
            mem_hasht_pr_is_deleted(pair),
            mem_hasht_pr_is_corrupt(pair),
        };
        for (int i=0; i<3; i++) {
            if (((expect[i] > 0) && !found[i]) || ((expect[i] < 0) && found[i]))
                return false;
        }
    }
    return true;
}

static bool mem_hasht_dbg_sanity_01(struct mem_hasht *ht) {
    return ht->tab &&
           ht->nbuckets &&
           (ht->nbuckets_po2 == mem_hasht_get_adiv_power_idx(ht->nbuckets)) &&
           (ht->shrink_at_lt_n < ht->grow_at_gt_n) &&
           ht->div_func;
}
static bool mem_hasht_dbg_sanity_heavy(struct mem_hasht *ht) {
    return mem_hasht_dbg_sanity_01(ht) && mem_hasht_dbg_check(ht, 0, ht->nbuckets, 0, 0, -1);
}

static void mem_hasht_memset(struct mem_hasht *ht, long begin_inc, long end_exc) {
    MEM_HASHT_ASSERT(mem_hasht_dbg_sanity_01(ht), "mem_hasht corrupt or not initialized");
    //the flags are designed so that memsetting with 0 means: empty, not deleted, not corrupt
    memset(ht->tab + begin_inc, 0, sizeof(struct mem_hasht_pair_type) * (end_exc - begin_inc));
    MEM_HASHT_ASSERT(mem_hasht_dbg_check(ht, begin_inc, end_exc, 1, -1, -1), "");
}

static int mem_hasht_init_ex(struct mem_hasht *ht,
                        long initial_nelements, 
                        mem_hasht_malloc_fptr alloc,
                        mem_hasht_realloc_fptr realloc,
                        mem_hasht_free_fptr free,
                        void *alloc_userdata,
                        long shrink_at_percentage,
                        long grow_at_percentage)
{
    int rv;
#ifdef MEM_HASHT_DBG
    memset(ht, 0x3c, sizeof *ht);
#endif
    const struct mem_hasht_alloc_funcs memfuncs = { alloc, realloc, free, alloc_userdata, };
    ht->memfuncs = memfuncs;
    ht->nelements = 0;
    ht->ndeleted = 0;
    ht->nbuckets_po2 = 0;

    rv = mem_hasht_init_parameters(ht, shrink_at_percentage, grow_at_percentage);
    if (rv != MEM_HASHT_OK)
        return rv;

    long initial_nbuckets = mem_hasht_calc_nelements_to_nbuckets(initial_nelements, ht->shrink_at_percentage, ht->grow_at_percentage);
    rv = mem_hasht_change_sz_field(ht, initial_nbuckets, false);
    if (rv != MEM_HASHT_OK)
        return rv;

    ht->tab = ht->memfuncs.alloc(sizeof(struct mem_hasht_pair_type) * ht->nbuckets, ht->memfuncs.userdata);
    if (!ht->tab)
        return MEM_HASHT_ALLOC_ERR;
    mem_hasht_memset(ht, 0, ht->nbuckets); //mark everything empty
    return MEM_HASHT_OK;
}

//returns an empty copy that has the same allocator settings and same parameters
static int mem_hasht_init_copy_settings(struct mem_hasht *ht,
                        long initial_nelements, 
                        const struct mem_hasht *source)
{
    return mem_hasht_init_ex(ht, //struct mem_hasht *ht,
                        initial_nelements, //long initial_nelements, 
                        source->memfuncs.alloc,//mem_hasht_malloc_fptr alloc,
                        source->memfuncs.realloc,//mem_hasht_realloc_fptr realloc,
                        source->memfuncs.free,// mem_hasht_free_fptr free,
                        source->memfuncs.userdata, //void *alloc_userdata,
                        source->shrink_at_percentage, //long shrink_at_percentage,
                        source->grow_at_percentage //long grow_at_percentage)
                        );
}

static int mem_hasht_init(struct mem_hasht *ht, long initial_nelements) {
    return mem_hasht_init_ex(ht, //struct mem_hasht *ht,
                        initial_nelements, //long initial_nelements, 
                        mem_hasht_def_malloc,//mem_hasht_malloc_fptr alloc,
                        mem_hasht_def_realloc,//mem_hasht_realloc_fptr realloc,
                        mem_hasht_def_free,// mem_hasht_free_fptr free,
                        NULL, //void *alloc_userdata,
                        20, //long shrink_at_percentage,
                        60 //long grow_at_percentage)
                        );
}
static void mem_hasht_deinit(struct mem_hasht *ht) {
    ht->memfuncs.free(ht->tab, ht->memfuncs.userdata);
    ht->tab = NULL;
    mem_hasht_zero_sz_field(ht);
}
static long mem_hasht_integer_mod_buckets(struct mem_hasht *ht, size_t full_hash) {
    long divd_hash = ht->div_func(full_hash); //fast division (% not division) by hardcoded primes
    MEM_HASHT_ASSERT(divd_hash < ht->nbuckets, "");
    return divd_hash;
}

//precondition: idx can only be in [-1...nbuckets] (inclusive both ends)
static long mem_hasht_idx_mod_buckets(struct mem_hasht *ht, long idx) {
    MEM_HASHT_ASSERT(idx >= -1 && idx <= ht->nbuckets, "");
    if (idx < 0)
        return ht->nbuckets - 1;
    if (idx >= ht->nbuckets)
        return 0;
    return idx;
}

static long mem_hasht_n_unused_buckets(struct mem_hasht *ht) {
    return ht->nbuckets - ht->nelements;
}
static long mem_hasht_n_empty_buckets(struct mem_hasht *ht) {
    return ht->nbuckets - ht->nelements - ht->ndeleted;
}
static long mem_hasht_n_nonempty_buckets(struct mem_hasht *ht) {
    return ht->nelements + ht->ndeleted;
}
//excluding deleted
static long mem_hasht_n_used_buckets(struct mem_hasht *ht) {
    return ht->nelements;
}

//returns 0 if equal
static int mem_hasht_cmp(struct mem_hasht *ht, mem_hasht_key_type *key1, unsigned int partial_hash_1, struct mem_hasht_pair_type *pair) {
    (void) ht;

    //skip full key comparison
    if (mem_hasht_pr_get_partial_hash(pair) != partial_hash_1)
        return 1; 

    MEM_HASHT_ASSERT(mem_hasht_key_eq_cmp(&pair->key, &pair->key) == 0,
                                  "mem_hasht_key_eq_cmp() is broken, testing it on the same key fails to report it's equal to itself");
    return mem_hasht_key_eq_cmp(key1, &pair->key);
}

//on successful match, returns MEM_HASHT_OK
//otherwise unless an error occurs it returns NOT_FOUND and out_idx will hold a suggested place to insert 
//if we have no suggested place then out_idx is set to NOT_FOUND too
static inline int mem_hasht_find_pos__(struct mem_hasht *ht, mem_hasht_key_type *key, long *out_idx, size_t *full_hash_out) {
    MEM_HASHT_ASSERT(out_idx && full_hash_out, "");
    size_t full_hash = mem_hasht_hash(key);
    unsigned int partial_hash = mem_hasht_hash_to_partial_hash(full_hash);
    *full_hash_out = full_hash;
    long idx = mem_hasht_integer_mod_buckets(ht, full_hash);
    long suggested = MEM_HASHT_NOT_FOUND; //suggest where to insert

    if (mem_hasht_n_empty_buckets(ht) < 1) {
        //note that if mem_hasht_n_unused_buckets is anywhere near one it'll be a very a slow search anyways
        MEM_HASHT_ASSERT(false, "precondition violated, this leads to an infinite loop");
        return MEM_HASHT_INVALID_TABLE_STATE;
    }

    //we can probably use an upper iteration count, in case there is corruption, but we just ignore that here, we assume the user is sane
    while (1) {
        struct mem_hasht_pair_type *pair = ht->tab + idx;
        if (mem_hasht_pr_is_occupied(pair)) {
            if (mem_hasht_cmp(ht, key, partial_hash, pair) == 0) {
                *out_idx = idx;
                return MEM_HASHT_OK; //found
            }
        }
        else if (mem_hasht_pr_is_deleted(pair)) {
            if (suggested == MEM_HASHT_NOT_FOUND)
                suggested = idx; 
        }
        else if (mem_hasht_pr_is_empty(pair)) {
            if (suggested == MEM_HASHT_NOT_FOUND)
                suggested = idx;
            *out_idx = suggested;
            return MEM_HASHT_NOT_FOUND;
        }
#ifdef MEM_HASHT_DBG
        else {
            MEM_HASHT_ASSERT(false, "invalid bucket state");
        }
#endif
        idx = mem_hasht_idx_mod_buckets(ht, idx + 1); //this is where we can change linear probing
    }

    //unreachable
    *out_idx = MEM_HASHT_NOT_FOUND;
    return MEM_HASHT_INVALID_TABLE_STATE;
}

//fwddecl
static int mem_hasht_init_copy_settings(struct mem_hasht *ht, long initial_nelements, const struct mem_hasht *source);
static int mem_hasht_insert(struct mem_hasht *ht, mem_hasht_key_type *key, mem_hasht_value_type *value);

static bool mem_hasht_index_within(long start_idx, long cursor_idx, long end_idx_inclusive) {
    if ((start_idx <= end_idx_inclusive && cursor_idx >  end_idx_inclusive                             ) ||
        (start_idx >  end_idx_inclusive && cursor_idx <=  start_idx && cursor_idx >  end_idx_inclusive)  ||
        (cursor_idx == start_idx)                                                                           )
        return false;
    return true;
}
//returns a negative value if it finds none
//in first iteration cursor_idx must be MEM_HASHT_ITER_FIRST, this is used to tell the difference between whether we wrapped around or not
static long mem_hasht_skip_to_next__(struct mem_hasht *ht, long start_idx, long cursor_idx, long end_idx_inclusive) {
    if (ht->nelements == 0) {
        return MEM_HASHT_ITER_STOP;
    }
    else if (cursor_idx == MEM_HASHT_ITER_FIRST) {
        cursor_idx = start_idx;
    }
    else {
        cursor_idx = mem_hasht_idx_mod_buckets(ht, cursor_idx + 1); 
        if (!mem_hasht_index_within(start_idx, cursor_idx, end_idx_inclusive))
            return MEM_HASHT_ITER_STOP;
    }

    MEM_HASHT_ASSERT(end_idx_inclusive >= 0 && end_idx_inclusive < ht->nbuckets, "");
    MEM_HASHT_ASSERT(cursor_idx >= 0  &&  cursor_idx < ht->nbuckets, "");
    MEM_HASHT_ASSERT(start_idx >= 0  &&  start_idx < ht->nbuckets, "");
    for (long i=0; i<ht->nbuckets; i++) {
        struct mem_hasht_pair_type *pair = ht->tab + cursor_idx;
        if (mem_hasht_pr_is_occupied(pair)) {
            return cursor_idx;
        }
        cursor_idx = mem_hasht_idx_mod_buckets(ht, cursor_idx + 1); 
        if (!mem_hasht_index_within(start_idx, cursor_idx, end_idx_inclusive))
            return MEM_HASHT_ITER_STOP;
    }
    return MEM_HASHT_INVALID_TABLE_STATE;
}

static int mem_hasht_copy_all_to(struct mem_hasht *destination, struct mem_hasht *source) {
    MEM_HASHT_ASSERT(destination != source && source && destination, "");
    int rv;
    long idx = mem_hasht_skip_to_next__(source, 0, MEM_HASHT_ITER_FIRST, source->nbuckets - 1);
    while (idx >= 0) {
        struct mem_hasht_pair_type *pair = source->tab + idx;
        rv = mem_hasht_insert(destination, &pair->key, &pair->value);
        if (rv != MEM_HASHT_OK)
            return rv; //failed in middle of copying
        idx = mem_hasht_skip_to_next__(source, 0, idx, source->nbuckets - 1);
    }
    if (idx != MEM_HASHT_ITER_STOP)
        return (int) idx; //failed somehow
    return MEM_HASHT_OK;
}

static int mem_hasht_resize__(struct mem_hasht *ht, long new_element_count) {

    long new_bucket_count = mem_hasht_calc_nelements_to_nbuckets(new_element_count, ht->shrink_at_percentage, ht->grow_at_percentage);
    if (ht->nbuckets_po2 == mem_hasht_get_adiv_power_idx(new_bucket_count)) {
        return MEM_HASHT_OK; 
        //because we use primes, for some reason both new value and old values map to the same power of two
        //and there is no point in resizing, since this is an approximate thing it's not a big deal
    }
    MEM_HASHT_ASSERT(new_bucket_count > MEM_HASHT_MIN_TABLESIZE, "");

    struct mem_hasht new_ht;
    int rv = mem_hasht_init_copy_settings(&new_ht, new_bucket_count, ht);
    if (rv != MEM_HASHT_OK) {
        return rv;
    }
    rv = mem_hasht_copy_all_to(&new_ht, ht);
    if (rv != MEM_HASHT_OK) {
        mem_hasht_deinit(&new_ht);
        return rv;
    }
    MEM_HASHT_ASSERT(new_ht.nelements == ht->nelements, "copying failed");
    MEM_HASHT_ASSERT(new_ht.ndeleted == 0, "copying failed");

    //swap and deinit
    mem_hasht_deinit(ht);
    memcpy(ht, &new_ht, sizeof *ht);

    MEM_HASHT_ASSERT(mem_hasht_dbg_sanity_heavy(ht), "");

    return MEM_HASHT_OK;
}

enum mem_hasht_hint {
    MEM_HASHT_HINT_NONE,
    MEM_HASHT_HINT_INSERTING,
    MEM_HASHT_HINT_DELETING,
};
static int mem_hasht_if_needed_try_resize(struct mem_hasht *ht, int hint) {
    int rv = MEM_HASHT_OK;
    //avoids trying to shrink when we're inserting, and avoids trying to grow when we're removing elements
    if (ht->nelements >= ht->grow_at_gt_n && (hint != MEM_HASHT_HINT_DELETING)) {
        rv = mem_hasht_resize__(ht, ht->nelements);
    }
    else if ((ht->nelements < ht->shrink_at_lt_n) && ((ht->nbuckets / 2) < MEM_HASHT_MIN_TABLESIZE) && (hint != MEM_HASHT_HINT_INSERTING)) {
        rv = mem_hasht_resize__(ht, ht->nelements);
    }
    return rv;
}
static bool mem_hasht_at_insert_must_resize(struct mem_hasht *ht) {
    //we need to have at least one empty bucket, otherwise we can run into an infinite loop while searching
    return (mem_hasht_n_unused_buckets(ht) <= 1); 
}

//clears flags, makes it occupied, copies key and value to it
static int mem_hasht_set_pair_at_pos__(struct mem_hasht *ht, size_t full_hash, mem_hasht_key_type *key, mem_hasht_value_type *value, long place_to_insert_idx) {
    MEM_HASHT_ASSERT(ht->nelements < ht->nbuckets, "");
    MEM_HASHT_ASSERT(place_to_insert_idx >= 0 && place_to_insert_idx < ht->nbuckets , "");
    struct mem_hasht_pair_type *pair = ht->tab + place_to_insert_idx;
    pair->pair_data = mem_hasht_pr_combine_flags_and_partial_hash(MEM_HASHT_VLT_IS_NOT_EMPTY, //flags
                                                        mem_hasht_hash_to_partial_hash(full_hash));
    memcpy(&pair->key, key, sizeof *key);
    memcpy(&pair->value, value, sizeof *value);
    return MEM_HASHT_OK;
}

static int mem_hasht_insert__(struct mem_hasht *ht, mem_hasht_key_type *key, mem_hasht_value_type *value, long *found_idx_out, bool or_replace) {
    MEM_HASHT_ASSERT(mem_hasht_dbg_sanity_01(ht), "mem_hasht corrupt or not initialized");
    MEM_HASHT_ASSERT(ht->nelements < ht->nbuckets, "");
    MEM_HASHT_ASSERT(found_idx_out, "");
    long found_idx;
    int rv = mem_hasht_if_needed_try_resize(ht, MEM_HASHT_HINT_INSERTING);
    if (rv != MEM_HASHT_OK && mem_hasht_at_insert_must_resize(ht)) {
        //failed, translate the error
        if (rv == MEM_HASHT_INVALID_TABLE_STATE || rv == MEM_HASHT_ALLOC_ERR)
            return rv;
        else
            return MEM_HASHT_FAILED_AT_RESIZE;
    }

    size_t full_hash;
    rv = mem_hasht_find_pos__(ht, key, &found_idx, &full_hash);

    if (found_idx == MEM_HASHT_NOT_FOUND) {
        //weird error, we were expecting either:
        //an index (if found)
        //a suggestion index (if not found)
        *found_idx_out = MEM_HASHT_NOT_FOUND;
        return MEM_HASHT_INVALID_TABLE_STATE;
    }

    if (rv == MEM_HASHT_OK && !or_replace) {
        //found duplicate, and we're not told to replace it
        *found_idx_out = found_idx;
        return MEM_HASHT_DUPLICATE_KEY;
    }
    else if (rv == MEM_HASHT_NOT_FOUND) {
        //not a duplicate, new element
        struct mem_hasht_pair_type *pair = ht->tab + found_idx; 
        if (mem_hasht_pr_is_deleted(pair)) {
            MEM_HASHT_ASSERT(ht->ndeleted > 0, "found a deleted element even though ht->ndeleted <= 0");
            ht->ndeleted--;
        }
        ht->nelements++;
    }
    else if (rv != MEM_HASHT_OK) {
        //some other error, return it
        *found_idx_out = MEM_HASHT_NOT_FOUND;
        return rv;
    }

    rv = mem_hasht_set_pair_at_pos__(ht, full_hash, key, value, found_idx);
    *found_idx_out = found_idx;
    return rv;
}


//this only happens when we delete an element where the one next to it is empty:
// [filled] [filled] [filled and to be deleted] [empty]
//                    ^^^mark as empty^^^^^^     ^next^
//this _doesnt_ happen when we delete an element where the one next to it is filled or deleted:
// [filled] [filled] [filled and to be deleted] [filled or deleted] [filled] [empty]
//                    ^^^mark as deleted^^^^     ^next^
static void mem_hasht_mark_as_empty__(struct mem_hasht *ht, long at_index) {
    struct mem_hasht_pair_type *pair = ht->tab + at_index; 
    MEM_HASHT_ASSERT(!mem_hasht_pr_is_empty(pair), "");
    mem_hasht_pr_set_flags(pair,
                  (mem_hasht_pr_flags(pair) & (~ (MEM_HASHT_VLT_IS_NOT_EMPTY | MEM_HASHT_VLT_IS_DELETED))));
    MEM_HASHT_ASSERT(mem_hasht_pr_is_empty(pair), "");
}
static void mem_hasht_mark_as_occupied__(struct mem_hasht *ht, long at_index) {
    struct mem_hasht_pair_type *pair = ht->tab + at_index; 
    MEM_HASHT_ASSERT(mem_hasht_pr_is_empty(pair) || mem_hasht_pr_is_deleted(pair), "");
    mem_hasht_pr_set_flags(pair,
                  (mem_hasht_pr_flags(pair) & (~MEM_HASHT_VLT_IS_DELETED)) | MEM_HASHT_VLT_IS_NOT_EMPTY);
    MEM_HASHT_ASSERT(!mem_hasht_pr_is_empty(pair), "");
}
static void mem_hasht_mark_as_deleted__(struct mem_hasht *ht, long at_index) {
    struct mem_hasht_pair_type *pair = ht->tab + at_index; 
    MEM_HASHT_ASSERT(mem_hasht_pr_is_occupied(pair), "trying to delete an empty element");
    mem_hasht_pr_set_flags(pair,
                  mem_hasht_pr_flags(pair) | MEM_HASHT_VLT_IS_DELETED);
    MEM_HASHT_ASSERT(mem_hasht_pr_is_deleted(pair), "");
}

static int mem_hasht_remove(struct mem_hasht *ht, mem_hasht_key_type *key) {
    long found_idx;
    size_t full_hash;
    int rv = mem_hasht_find_pos__(ht, key, &found_idx, &full_hash);
    if (rv == MEM_HASHT_NOT_FOUND) {
        return rv;
    }
    else if (rv != MEM_HASHT_OK) {
        //failed, TODO: check what's the error
        return rv;
    }
    MEM_HASHT_ASSERT(found_idx >= 0 && found_idx < ht->nbuckets, "find pos returned invalid index");
#ifdef MEM_HASHT_DBG
        struct mem_hasht_pair_type *pair = ht->tab + found_idx;
        MEM_HASHT_ASSERT(mem_hasht_pr_is_occupied(pair), "find pos returned an index of a deleted/empty element");
#endif // MEM_HASHT_DBG

    //optimization: if next element is empty, mark our element as empty too, otherwise mark our element as deleted
    //TODO: benchmark this
    long next_idx = mem_hasht_idx_mod_buckets(ht, found_idx + 1); //this assumes linear probing
    struct mem_hasht_pair_type *next_pair = ht->tab + next_idx;
    //TODO, division even though it is fast can be optimized to be a branch in wrap around cases
    //After benchmarking this, the results were: cleaning up in general made things faster by 2.0%
    //MEM_HASHT_AGRESSIVE_CLEANUP made things faster by about 0.5% (which is insignificant)
    if (mem_hasht_pr_is_empty(next_pair)) {
        mem_hasht_mark_as_empty__(ht, found_idx);

        //^TODO: add tests that extensively test the table state after lots of deletions
        //
        //when we have the situation:
        //                          [deleted] [deleted] [deleted] [filled]                      [empty] [filled] [empty] [deleted]
        //                               ^        ^       ^         ^the item we just deleted    ^        ^^^dont care^^^^^^^^^^
        //                               ^^^^^^^^^^^^^^^^^^^                                     ^must be empty
        //                             ^^^^it doesn't matter to what hashes those keys were originally from (their probe length)
        //                             ^^^^as long as they're deleted (And followd by an empty bucket)
        //                             ^^^^we can mark them empty
        //
        //
        //what was                  [deleted] [deleted] [deleted] [filled] [empty]
        //becomes:                  [deleted] [deleted] [deleted] [empty]  [empty]
        //                          [deleted] [deleted] [empty]   [empty]  [empty]
        //                          [deleted] [empty]   [empty]   [empty]  [empty]
        //                          [empty]   [empty]   [empty]   [empty]  [empty]
        //this an attempt to speed up searches after deletions
        
        #define MEM_HASHT_AGRESSIVE_CLEANUP
        #ifdef MEM_HASHT_AGRESSIVE_CLEANUP
            long prev_idx = mem_hasht_idx_mod_buckets(ht, found_idx - 1);
            struct mem_hasht_pair_type *prev_pair = ht->tab + prev_idx;
            while (mem_hasht_pr_is_deleted(prev_pair)) {
                mem_hasht_mark_as_empty__(ht, prev_idx);
                MEM_HASHT_ASSERT(mem_hasht_pr_is_empty(prev_pair), "");
                prev_idx = mem_hasht_idx_mod_buckets(ht, prev_idx - 1); 
                prev_pair = ht->tab + prev_idx;
            }
        #else
            long supposed_to_be_in_idx = mem_hasht_integer_mod_buckets(ht, full_hash);
            long probe_len = found_idx - supposed_to_be_in_idx;
            if (probe_len < 0)
                probe_len = probe_len + ht->nbuckets;
            long prev_idx = mem_hasht_idx_mod_buckets(ht, found_idx - 1);
            struct mem_hasht_pair_type *prev_pair = ht->tab + prev_idx;
            for (long i = 0; i < probe_len && mem_hasht_pr_is_deleted(prev_pair); i++) {
                mem_hasht_mark_as_empty__(ht, prev_idx);
                MEM_HASHT_ASSERT(mem_hasht_pr_is_empty(prev_pair), "");
                prev_idx = mem_hasht_idx_mod_buckets(ht, prev_idx - 1); 
                prev_pair = ht->tab + prev_idx;
            }
        #endif // MEM_HASHT_AGRESSIVE_CLEANUP
    }
    else {
        mem_hasht_mark_as_deleted__(ht, found_idx);
        ht->ndeleted++;
    }

    ht->nelements--;
    return MEM_HASHT_OK;
}
static int mem_hasht_insert(struct mem_hasht *ht, mem_hasht_key_type *key, mem_hasht_value_type *value) {
    long idx_unused;
    int rv = mem_hasht_insert__(ht, key, value, &idx_unused, false /*dont replace*/);
    return rv;
}
struct mem_hasht_iter {
    long started_at_idx;
    long current_idx;
    //public field
    //the two members: pair->key and pair->value can be accessed directly (assuming a valid iterator)
    struct mem_hasht_pair_type *pair; 
};
static struct mem_hasht_iter mem_hasht_mk_invalid_iter(void) {
    struct mem_hasht_iter iter = {MEM_HASHT_ITER_STOP, MEM_HASHT_ITER_STOP, NULL};
    return iter;
}
static struct mem_hasht_iter mem_hasht_mk_iter(long start_idx, struct mem_hasht_pair_type *pair) {
    struct mem_hasht_iter iter = {start_idx, MEM_HASHT_ITER_FIRST, pair};
    return iter;
}
static bool mem_hasht_iter_check(struct mem_hasht_iter *iter) {
    MEM_HASHT_ASSERT((iter->current_idx == MEM_HASHT_ITER_STOP) || (iter->pair != NULL && iter->current_idx >= 0), "invalid iterator state");
    return iter->current_idx != MEM_HASHT_ITER_STOP;
}

static int mem_hasht_begin_iterator(struct mem_hasht *ht, struct mem_hasht_iter *iter) {
    long next_idx = mem_hasht_skip_to_next__(ht, 0, MEM_HASHT_ITER_FIRST, ht->nbuckets - 1);
    if (next_idx < 0) {
        *iter = mem_hasht_mk_invalid_iter();
        return MEM_HASHT_ITER_STOP;
    }
    iter->started_at_idx = 0;
    iter->current_idx = next_idx;
    iter->pair = ht->tab + next_idx;
    return MEM_HASHT_OK;
}

static int mem_hasht_iter_next(struct mem_hasht *ht, struct mem_hasht_iter *iter) {
    MEM_HASHT_ASSERT((iter->current_idx == MEM_HASHT_ITER_FIRST) ||
                 (iter->current_idx >= 0 && iter->current_idx < ht->nbuckets), "invalid iterator");

    if (iter->current_idx == MEM_HASHT_ITER_STOP)
        return MEM_HASHT_ITER_STOP; //the caller will probably be stuck in an infinite loop, that's what you get for not checking return value

    long next_idx = mem_hasht_skip_to_next__(ht, iter->started_at_idx, iter->current_idx, ht->nbuckets - 1);
    if (next_idx < 0) {
        *iter = mem_hasht_mk_invalid_iter();
        return MEM_HASHT_ITER_STOP;
    }
    iter->current_idx = next_idx;
    iter->pair = ht->tab + next_idx;
    return MEM_HASHT_OK;
}
static int mem_hasht_find(struct mem_hasht *ht, mem_hasht_key_type *key, struct mem_hasht_iter *out) {
    long found_idx;
    size_t full_hash_unused;
    int rv = mem_hasht_find_pos__(ht, key, &found_idx, &full_hash_unused);
    if (rv == MEM_HASHT_NOT_FOUND) {
        *out = mem_hasht_mk_invalid_iter();
        return rv;
    }
    else if (rv != MEM_HASHT_OK) {
        //failed, TODO: check what's the error
        *out = mem_hasht_mk_invalid_iter();
        return rv;
    }
    MEM_HASHT_ASSERT(found_idx >= 0 && found_idx < ht->nbuckets, "find pos returned invalid index");
    struct mem_hasht_pair_type *pair = ht->tab + found_idx;
    *out = mem_hasht_mk_iter(found_idx, pair);
    return MEM_HASHT_OK;
}
static int mem_hasht_find_or_insert(struct mem_hasht *ht, mem_hasht_key_type *key, mem_hasht_value_type *value, struct mem_hasht_iter *out) {
    long found_idx;
    int rv = mem_hasht_insert__(ht, key, value, &found_idx, true /*do replace*/);
    if (rv == MEM_HASHT_OK) {
        struct mem_hasht_pair_type *pair = ht->tab + found_idx;
        *out = mem_hasht_mk_iter(found_idx, pair);
    }
    else {
        *out = mem_hasht_mk_invalid_iter();
    }
    return rv;
}

