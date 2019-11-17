#ifndef KNIT_OBJ_H
#define KNIT_OBJ_H
/*hashtable related functions*/
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
    static int knit_mem_hasht_key_eq_cmp(knit_mem_hasht_key_type *key_1, knit_mem_hasht_key_type *key_2) {
        //we are comparing two void pointers
        if (*key_1 == *key_2)
            return 0;  //eq
        return 1;
    }
    static size_t knit_mem_hasht_hash(knit_mem_hasht_key_type *key) {
        return (uintptr_t) *key; //casting a void ptr to an integer
    }
    //fwd
    static size_t knitx_obj_hash(struct knit *knit, struct knit_obj *obj);
    static int knitx_obj_eq(struct knit *knit, struct knit_obj *obj_a, struct knit_obj *obj_b);
    static void knit_assert_h(int condition, const char *fmt, ...);
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
/*end*/

static struct knit_str *knit_as_str(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_STR, "knit_as_str(): invalid argument type, expected string");
    return &obj->u.str;
}
static struct knit_list *knit_as_list(struct knit_obj *obj) {
    knit_assert_h(obj->u.ktype == KNIT_LIST, "knit_as_list(): invalid argument type, expected list");
    return &obj->u.list;
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

//fwd
static int knitx_str_new_copy(struct knit *knit, struct knit_str **strp, struct knit_str *src);
static int knitx_int_new(struct knit *knit, struct knit_int **integerp_out, int value);
//TODO make sure dest,src order is consistent in functions
//does a shallow copy
static int knitx_obj_copy(struct knit *knit, struct knit_obj **dest, struct knit_obj *src) {
    int rv = KNIT_OK;
    struct knit_int *new_int;
    struct knit_str *new_str;
    switch (src->u.ktype) {
        case KNIT_INT:
            rv = knitx_int_new(knit, &new_int, src->u.integer.value); KNIT_CRV(rv);
            *dest = (struct knit_obj *)new_int;
            return KNIT_OK;
        case KNIT_STR:
            rv = knitx_str_new_copy(knit, &new_str, (struct knit_str *)src); KNIT_CRV(rv);
            *dest = (struct knit_obj *)new_str;
            return KNIT_OK;
        default:
            *dest = NULL;
            return knit_error(knit, KNIT_RUNTIME_ERR, "copying %s types is not implemented", knitx_obj_type_name(knit, src));
    }
    kunreachable();
    return KNIT_RUNTIME_ERR;
}


static inline int knitx_op_do_test_binop(struct knit *knit, struct knit_obj *a, struct knit_obj *b, int op); //fwd
//boolean, 1 means eq, 0 uneq
static int knitx_obj_eq(struct knit *knit, struct knit_obj *obj_a, struct knit_obj *obj_b) {
    int rv = knitx_op_do_test_binop(knit, obj_a, obj_b, KTESTEQ);
    if (rv == KNIT_OK && knit->ex.last_cond) {
        return 1;
    }
    return 0;
}

static int knitx_list_init(struct knit *knit, struct knit_list *list, int isz) {
    int rv = KNIT_OK;
    if (isz < 0)
        isz = 0;
    void *p = NULL;
    if (isz > 0) {
        rv = knitx_tmalloc(knit, sizeof(struct knit_obj *) * isz, &p); KNIT_CRV(rv);
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
static int knitx_list_new(struct knit *knit, struct knit_list **list, int isz) {
    int rv = KNIT_OK;
    void *p;
    rv = knitx_tmalloc(knit, sizeof(struct knit_list), &p); KNIT_CRV(rv);
    rv = knitx_list_init(knit, p, isz); 
    if (rv != KNIT_OK) {
        knitx_tfree(knit, p);
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
        rv = knitx_list_resize(knit, list, list->cap == 0 ? 8 : list->cap * 2); KNIT_CRV(rv);
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
    rv = kobj_hasht_init(&dict->ht, isz);

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
static int knitx_dict_new(struct knit *knit, struct knit_dict **dict, int isz) {
    int rv = KNIT_OK;
    void *p;
    rv = knitx_tmalloc(knit, sizeof(struct knit_dict), &p); KNIT_CRV(rv);
    rv = knitx_dict_init(knit, p, isz); 
    if (rv != KNIT_OK) {
        knitx_tfree(knit, p);
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
        rv = knitx_obj_copy(knit, &new_key, key); KNIT_CRV(rv);
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


//see str valid states at kdata.h
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
static int knitx_str_destroy(struct knit *knit, struct knit_str *strp) {
    int rv = knitx_str_deinit(knit, strp);
    int rv2 = knitx_tfree(knit, strp);
    if (rv != KNIT_OK)
        return rv;
    return rv2;
}
static int knitx_str_strcpy(struct knit *knit, struct knit_str *str, const char *src0);
static int knitx_str_new_strcpy(struct knit *knit, struct knit_str **strp, const char *src0) {
    int rv = knitx_str_new(knit, strp); KNIT_CRV(rv);
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
    int rv = knitx_str_init(knit, str); KNIT_CRV(rv);
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
//exclusive end, inclusive begin
static int knitx_str_mutsubstr(struct knit *knit, struct knit_str *str, int begin, int end) {
    knit_assert_h((begin <= end) && (begin <= str->len) && (end <= str->len), "invalid arguments to mutsubstr()");
    void *p = NULL;
    int len = end - begin;
    int rv  = knitx_tmalloc(knit, len + 1, &p); KNIT_CRV(rv);
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
//assumes the length of both was checked and it was equal!
static int knit_strl_eq(const char *a, const char *b, size_t len) {
    return memcmp(a, b, len) == 0;

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



//"obj".property
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
    rv = knitx_str_clear(knit, outi_str); KNIT_CRV(rv);
    if (obj == NULL) {
        rv = knitx_str_strcpy(knit, outi_str, "[NULL]"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_STR) {
        struct knit_str *objstr = (struct knit_str *) obj;
        if (!human) {
            rv = knitx_str_strlcpy(knit, outi_str, "\"", 1); KNIT_CRV(rv);
        }
        rv = knitx_str_strlappend(knit, outi_str, objstr->str, objstr->len); KNIT_CRV(rv);
        if (!human) {
            rv = knitx_str_strlappend(knit, outi_str, "\"", 1); KNIT_CRV(rv);
        }
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
            rv = knitx_obj_rep(knit, objlist->items[i], tmpstr, 0);
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
    else if (obj->u.ktype == KNIT_DICT) {
        struct knit_dict *objdict = (struct knit_dict *) obj;
        rv = knitx_str_strlcpy(knit, outi_str, "{", 1); KNIT_CRV(rv);
        struct kobj_hasht_iter iter;
        kobj_hasht_begin_iterator(&objdict->ht, &iter);
        for (int i=0; kobj_hasht_iter_check(&iter); i++, kobj_hasht_iter_next(&objdict->ht, &iter)) 
        {
            struct knit_obj *key = iter.pair->key;
            struct knit_obj *value = iter.pair->value;
            if (i) {
                rv = knitx_str_strlappend(knit, outi_str, ", ", 2); KNIT_CRV(rv);
            }
            struct knit_str *tmpstr = NULL;
            rv = knitx_str_new(knit, &tmpstr); KNIT_CRV(rv);

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
            KNIT_CRV(rv);
        }

        rv = knitx_str_strlappend(knit, outi_str, "}", 1); KNIT_CRV(rv);
        KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_CFUNC) {
        rv = knitx_str_strcpy(knit, outi_str, "<C function>"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_KFUNC) {
        rv = knitx_str_strcpy(knit, outi_str, "<knit function>"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_NULL) {
        rv = knitx_str_strcpy(knit, outi_str, "null"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_TRUE) {
        rv = knitx_str_strcpy(knit, outi_str, "true"); KNIT_CRV(rv);
    }
    else if (obj->u.ktype == KNIT_FALSE) {
        rv = knitx_str_strcpy(knit, outi_str, "false"); KNIT_CRV(rv);
    }
    else {
        rv = knitx_str_strcpy(knit, outi_str, "<unknown type>"); KNIT_CRV(rv);
        return KNIT_WARNING;
    }
    return KNIT_OK;
}
static int knitx_obj_dump(struct knit *knit, struct knit_obj *obj) {
    struct knit_str *tmpstr = NULL;
    int rv = knitx_str_new(knit, &tmpstr); KNIT_CRV(rv);
    rv = knitx_obj_rep(knit, obj, tmpstr, 0);
    if (rv != KNIT_OK) {
        knitx_str_destroy(knit, tmpstr);
        return rv;
    }
    fprintf(stderr, "%s\n", tmpstr->str);
    knitx_str_destroy(knit, tmpstr);
    return KNIT_OK;
}

#endif // KNIT_OBJ_H