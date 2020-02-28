#ifndef KNIT_GC
#define KNIT_GC

#include "kdata.h" //data structures


static int knitgc_walk_object(struct knit *knit, struct knit_obj *obj) {
    if (!obj)
        return;
    knitx_obj_dump(knit, obj);
    if (obj->u.ktype == KNIT_DICT) {
        struct knit_dict *dict = (struct knit_dict*) obj;
        struct kobj_hasht *ht = &dict->ht;
        struct kobj_hasht_iter iter;
        kobj_hasht_begin_iterator(ht, &iter);
        for (int i=0; kobj_hasht_iter_check(&iter); i++, kobj_hasht_iter_next(ht, &iter)) 
        {
            struct knit_obj *key = iter.pair->key;
            struct knit_obj *value = iter.pair->value;
            knitgc_walk_object(knit, key);
            knitgc_walk_object(knit, value);
        }
    }
    else if (obj->u.ktype == KNIT_LIST) {
        struct knit_list *list = (struct knit_list*) obj;
        for (int i=0; i<list->len; i++) {
            struct knit_obj *elem = list->items[i];
            knitgc_walk_object(knit, elem);
        }
    }
}

static int knitgc_get_bitset_buffer(struct knit *knit) {

}
static int knitgc_release_bitset_buffer(struct knit *knit) {
    
}

static int knitgc_walk_workingset(struct knit *knit) {
    struct knit_exec_state *exec_state = &knit->ex;
    struct knit_stack *stack = &knit->ex.stack;
    struct knit_vars_hasht *vars_ht = &knit->ex.global_ht;
    struct knit_objp_darray *stack_vals = &stack->vals;
    for (int i=0; i<stack_vals->len; i++) {
        knitgc_walk_object(knit, stack_vals->data[i]);
    }

    struct knit_vars_hasht_iter iter;
    knit_vars_hasht_begin_iterator(vars_ht, &iter);
    for (int i=0; knit_vars_hasht_iter_check(&iter); i++, knit_vars_hasht_iter_next(vars_ht, &iter)) 
    {
        struct knit_obj *value = iter.pair->value;
        knitgc_walk_object(knit, value);
    }
}

#endif //KNIT_GC
