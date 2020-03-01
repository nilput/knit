#ifndef KNIT_BITSET_H
#define KNIT_BITSET_H
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "kdata.h"
#include "knit_util.h"
#include "knit_bitset_data.h"

#if CHAR_BIT != 8
    #error "only supports 8 bit byte platforms"
#endif

#define BITS_IN_UNSIGNED (sizeof(unsigned) * 8)
#define UNSIGNED_ALL_BITS_ON UINT_MAX


struct idx_pair {
    size_t unsigned_idx;
    int bit_idx;
};
static struct idx_pair resolve_bit_idx(size_t bit_idx) {
    struct idx_pair idx;
    idx.unsigned_idx = (bit_idx / BITS_IN_UNSIGNED);
    idx.bit_idx = (bit_idx % BITS_IN_UNSIGNED);
    return idx;
}
static size_t recombine_bit_idx(size_t unsigned_idx, int bit_idx)
{
    return (unsigned_idx * BITS_IN_UNSIGNED) + bit_idx;
}
static int bits_excess (size_t bit_len) {
    if (!bit_len)
        return 0;
    struct idx_pair idx = resolve_bit_idx(bit_len - 1);
    return BITS_IN_UNSIGNED - idx.bit_idx - 1;
}

static size_t n_needed_unsigneds(size_t bit_len) {
    if (!bit_len)
        return 0;
    struct idx_pair idx = resolve_bit_idx(bit_len - 1);
    return idx.unsigned_idx + 1;
}

static void unsigned_set_bit(unsigned *u, int bit_idx, bool state)
{
    if (state)
        *u = *u | (1U << bit_idx);
    else
        *u = *u & (~ (1U << bit_idx) );
}
static bool unsigned_get_bit(unsigned *u, int bit_idx)
{
    return *u & (1U << bit_idx);
}


// a[bit] = b[bit] & (!a[bit])
void bitset_andn(struct knit_bitset *a, struct knit_bitset *b) {
    knit_assert_h(a->bit_len == b->bit_len, "");
    size_t n = n_needed_unsigneds(a->bit_len);
    for (long i=0; i<n; i++) {
        a->data[i] = b->data[i] & (~ a->data[i]);
    }
}

void bitset_set_all(struct knit_bitset *bitset, bool state, size_t up_to) {
    /*
    0000 0001
    0001 0001
    0101 0101
    1111 1111
    */
    if (up_to == 0) {
        up_to = bitset->bit_len;
    }
    unsigned char sb = state;
    sb |= sb << 4;
    sb |= sb << 2;
    sb |= sb << 1;
    memset(bitset->data, sb, sizeof(bitset->data[0]) * n_needed_unsigneds(bitset->bit_len));
}
int bitset_init(struct knit_bitset *bitset, size_t bit_len) 
{
    size_t sz = 0;
    unsigned *data = NULL;
    if (bit_len) {
       sz = n_needed_unsigneds(bit_len) * sizeof(unsigned);
       data = malloc(sz);
       if (!data) {
           return KNIT_NOMEM;
       }
        memset(data, 0, sz);
    }
    bitset->data = data;
    bitset->bit_len = bit_len;
    return KNIT_OK;
}
int bitset_realloc(struct knit_bitset *bitset, size_t new_bit_len)
{
    if (!new_bit_len) {
        bitset_deinit(bitset);
        return KNIT_OK;
    }
    if (!bitset->bit_len) {
        return bitset_init(bitset, new_bit_len);
    }
    struct idx_pair last_idx = resolve_bit_idx(bitset->bit_len - 1);
    last_idx.bit_idx++;
    unsigned *last_u = bitset->data + last_idx.unsigned_idx;
    for (int i=last_idx.bit_idx; i < BITS_IN_UNSIGNED; i++) {
        unsigned_set_bit(last_u, i, false);
    }

    size_t old_sz = (last_idx.unsigned_idx + 1) * sizeof(unsigned);
    size_t new_sz = n_needed_unsigneds(new_bit_len) * sizeof(unsigned);
    void *p = realloc(bitset->data, new_sz);
    if (!p) {
        return KNIT_NOMEM;
    }
    bitset->bit_len = new_bit_len;
    bitset->data = p;
    if (new_sz > old_sz) {
        memset(bitset->data + last_idx.unsigned_idx + 1, 0, new_sz - old_sz);
    }
    return KNIT_OK;
}
void bitset_deinit(struct knit_bitset *bitset) {
    free(bitset->data);
    bitset->data = NULL;
    bitset->bit_len = 0;
}

bool bitset_get_bit(struct knit_bitset *bitset, size_t bit_idx)
{
    struct idx_pair idx = resolve_bit_idx(bit_idx);
    return bitset->data[idx.unsigned_idx] & (1U << idx.bit_idx);
}
void bitset_set_bit(struct knit_bitset *bitset, size_t bit_idx, bool state)
{
    struct idx_pair idx = resolve_bit_idx(bit_idx);
    unsigned_set_bit(bitset->data + idx.unsigned_idx, idx.bit_idx, state);
}

long bitset_find_false_bit(struct knit_bitset *bitset,  size_t start_at_bit_idx)
{
    struct idx_pair last_idx = resolve_bit_idx(bitset->bit_len - 1);
    struct idx_pair start_idx = resolve_bit_idx(start_at_bit_idx);
    long start_at = start_idx.unsigned_idx; 
    long i;
    int bit_idx;
    unsigned inv_v;

    if (start_at_bit_idx != 0 && bitset->data[start_idx.unsigned_idx] != UNSIGNED_ALL_BITS_ON) {
        //check the first skipped unsigned (slow)
        for (i=start_idx.bit_idx; i < BITS_IN_UNSIGNED; i++) {
            if (!unsigned_get_bit(bitset->data + start_idx.unsigned_idx, i))
                return recombine_bit_idx(start_idx.unsigned_idx, i);
        }
        start_at++; //skip first unsigned
    }

    //find an unsigned that is not 0xFFFFFF...
    for (i = start_at; i <= last_idx.unsigned_idx; i++) {
        if (bitset->data[i] != UNSIGNED_ALL_BITS_ON) {
            goto found;
        }
    }
    return -1; //not found
found:
    bit_idx = 0;
    inv_v = ~ (bitset->data[i]);
    if (inv_v != 0) {
#ifdef _GNUC_
        //Built-in Function: int __builtin_ctz (unsigned int x)
        //Returns the number of trailing 0-bits in x, starting at the least significant bit position. If x is 0, the result is undefined.
        bit_idx = __builtin_ctz(inv_v);
#else
        for (bit_idx = 0; bit_idx<BITS_IN_UNSIGNED; bit_idx++) {
            if (inv_v & (1U << bit_idx))
                break;
        }
#endif
    }
    return recombine_bit_idx(i, bit_idx);
}

long bitset_find_true_bit(struct knit_bitset *bitset,  size_t start_at_bit_idx)
{
    struct idx_pair last_idx = resolve_bit_idx(bitset->bit_len - 1);
    struct idx_pair start_idx = resolve_bit_idx(start_at_bit_idx);
    long start_at = start_idx.unsigned_idx;
    long i;
    int bit_idx;
    unsigned v;

    if (start_at_bit_idx != 0 && bitset->data[start_idx.unsigned_idx] != 0) {
        //check the first skipped unsigned, (slow)
        for (i=start_idx.bit_idx; i < BITS_IN_UNSIGNED; i++) {
            if (unsigned_get_bit(bitset->data + start_idx.unsigned_idx, i))
                return recombine_bit_idx(start_idx.unsigned_idx, i);
        }
        start_at++; //skip first unsigned
    }

    //find an unsigned that is not 0x00000...
    for (i = start_at; i <= last_idx.unsigned_idx; i++) {
        if (bitset->data[i]) {
            goto found;
        }
    }
    return -1; //not found

found:
    bit_idx = 0;
    v = bitset->data[i];
#ifdef _GNUC_
    //Built-in Function: int __builtin_ctz (unsigned int x)
    //Returns the number of trailing 0-bits in x, starting at the least significant bit position. If x is 0, the result is undefined.
    bit_idx = __builtin_ctz(v);
#else
    for (bit_idx = 0; bit_idx<BITS_IN_UNSIGNED; bit_idx++) {
        if (v & (1U << bit_idx))
            break;
    }
#endif
    return recombine_bit_idx(i, bit_idx);
}

#endif