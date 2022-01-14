#ifndef KNIT_GC
#define KNIT_GC

#include "kdata.h" //data structures
#include "knit_bitset.h"

static void knit_obj_deinit(struct knit *knit, struct knit_obj *obj); //fwd

//size: number of objects
int knit_heap_init(struct knit *knit, struct knit_heap *heap, int heap_sz) {
    heap->capacity = heap_sz;
    heap->count = 0;
    int rv;
    if ((rv = bitset_init(&heap->alloc_bitset, heap_sz)) != 0) {
        return rv;
    }
    if ((rv = bitset_init(&heap->mark_bitset, heap_sz)) != 0) {
        bitset_deinit(&heap->alloc_bitset);
        return rv;
    }
    void *p;
    if ((rv = knitx_rmalloc(knit, heap_sz * sizeof(heap->objects[0]), &p)) != KNIT_OK) {
        bitset_deinit(&heap->alloc_bitset);
        bitset_deinit(&heap->mark_bitset);
        return rv;
    }
    heap->objects = p;
    return KNIT_OK;
}
void knit_heap_deinit(struct knit *knit, struct knit_heap *heap) {
    bitset_deinit(&heap->alloc_bitset);
    bitset_deinit(&heap->mark_bitset);
    knitx_rfree(knit, heap->objects);
}
struct knit_obj *knit_gc_new_object(struct knit *knit) {
    if (knit->ex.heap.count >= knit->ex.heap.capacity) {
        return NULL;
    }
    struct knit_bitset *b = &knit->ex.heap.alloc_bitset;
    long idx = bitset_find_false_bit(b, 0);
    knit_assert_h(idx >= 0, "");
    bitset_set_bit(b, idx, 1);
    knit->ex.heap.count++;
    return knit->ex.heap.objects + idx;
}

static long knit_gc_object_index(struct knit *knit, struct knit_obj *obj) {
    char *ptr = (char *) obj;
    char *heap_begin = (char *) knit->ex.heap.objects;
    char *heap_end   = (char *) (knit->ex.heap.objects + knit->ex.heap.capacity);
    if (ptr >= heap_begin &&
        ptr < heap_end      ) {
        return obj - knit->ex.heap.objects;
    }
    return -1;
}
static int knit_gc_is_gc_object(struct knit *knit, struct knit_obj *obj) {
    return knit_gc_object_index(knit, obj) != -1;
}



static void knit_gc_walk_object(struct knit *knit, struct knit_obj *obj) {
    if (!obj)
        return;
    long obj_idx = knit_gc_object_index(knit, obj);
    if (obj_idx != -1) {
        struct knit_bitset *mbs = &knit->ex.heap.mark_bitset;
        if (bitset_get_bit(mbs, obj_idx)) {
            return;
        }
        bitset_set_bit(mbs, obj_idx, 1);
    }
    else {
        #ifdef KNIT_DEBUG_GC
            fprintf(stderr, "Warning: object %p is not a gc object\n", (void *)obj);
        #endif
    }
    #ifdef KNIT_DEBUG_GC
        knitx_obj_dump(knit, obj);
    #endif
    if (obj->u.ktype == KNIT_DICT) {
        struct knit_dict *dict = (struct knit_dict*) obj;
        struct kobj_jadwal *ht = &dict->ht;
        struct kobj_jadwal_iter iter;
        kobj_jadwal_begin_iterator(ht, &iter);
        for (int i=0; kobj_jadwal_iter_check(&iter); i++, kobj_jadwal_iter_next(ht, &iter)) 
        {
            struct knit_obj *key = iter.pair->key;
            struct knit_obj *value = iter.pair->value;
            knit_gc_walk_object(knit, key);
            knit_gc_walk_object(knit, value);
        }
    }
    else if (obj->u.ktype == KNIT_LIST) {
        struct knit_list *list = (struct knit_list*) obj;
        for (int i=0; i<list->len; i++) {
            struct knit_obj *elem = list->items[i];
            knit_gc_walk_object(knit, elem);
        }
    }
    else if (obj->u.ktype == KNIT_KFUNC) {
        struct knit_kfunc *kfunc = (struct knit_kfunc*) obj;
        for (int i=0; i<kfunc->block.constants.len; i++) {
            struct knit_obj *elem = kfunc->block.constants.data[i];
            knit_gc_walk_object(knit, elem);
        }
    }
    
}

static int knit_gc_walk_workingset(struct knit *knit) {
    struct knit_exec_state *exec_state = &knit->ex;
    struct knit_stack *stack = &knit->ex.stack;
    struct knit_vars_jadwal *vars_ht = &knit->ex.global_ht;
    struct knit_objp_darray *stack_vals = &stack->vals;
    for (int i=0; i<stack_vals->len; i++) {
        knit_gc_walk_object(knit, stack_vals->data[i]);
    }

    struct knit_vars_jadwal_iter iter;
    knit_vars_jadwal_begin_iterator(vars_ht, &iter);
    for (int i=0; knit_vars_jadwal_iter_check(&iter); i++, knit_vars_jadwal_iter_next(vars_ht, &iter)) 
    {
        struct knit_obj *value = iter.pair->value;
        knit_gc_walk_object(knit, value);
    }
    return KNIT_OK;
}
static void knit_gc_obj_null(struct knit *knit, struct knit_obj *obj) {
    obj->u.ktype = KNIT_NULL;
}
static void knit_gc_cycle(struct knit *knit) {
    bitset_set_all(&knit->ex.heap.mark_bitset, 0, 0);
    knit_gc_walk_workingset(knit);
    bitset_andn(&knit->ex.heap.mark_bitset, &knit->ex.heap.alloc_bitset);
    //mark_bitset should contain dead objects
    struct knit_bitset *mbs = &knit->ex.heap.mark_bitset;
    long i = bitset_find_true_bit(mbs, 0);
    while (i != -1) {
        #ifdef KNIT_DEBUG_GC
        printf("Object %i is dead!\n", (int)i);
        knitx_obj_dump(knit, knit->ex.heap.objects + i);
        #endif
        struct knit_obj *obj = knit->ex.heap.objects + i;
        knit_obj_deinit(knit, obj);
        bitset_set_bit(&knit->ex.heap.alloc_bitset, i, 0);
        i = bitset_find_true_bit(mbs, i + 1);
        knit->ex.heap.count--;
    }
}

#endif //KNIT_GC
