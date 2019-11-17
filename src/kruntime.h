#include "knit_gc.h"
int knitx_str_strip(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    if (nargs != 1) { //1 for 'self'
        return knit_error(kstate, KNIT_NARGS, "knitx_str_strip() was called with a wrong number of arguments, expecting 0 argument");
    }
    struct knit_obj *self = NULL;
    int rv = knitx_get_arg(kstate, 0, &self); KNIT_CRV(rv);
    if (self->u.ktype != KNIT_STR) {
        return knit_error(kstate, KNIT_INVALID_TYPE_ERR, "knitx_str_strip(self, ...) was called with an unexpected type, expecting str");
    }

    struct knit_str *dupped = NULL;
    rv = knitx_str_new_copy(kstate, &dupped, (struct knit_str *) self); KNIT_CRV(rv);
    rv = knitx_str_mutstrip(kstate, dupped, " \n\t\f", KNITX_STRIP_LEFT | KNITX_STRIP_RIGHT); KNIT_CRV(rv);
    knitx_stack_rpush(kstate, &kstate->ex.stack, ktobj(dupped));

    knitx_creturns(kstate, 1);
    return KNIT_OK;
}
int knitx_list_append(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    if (nargs != 2) { //1 for 'self'
        return knit_error(kstate, KNIT_NARGS, "knitx_list_append(self, ...) was called with a wrong number of arguments, expecting 1 argument");
    }
    struct knit_obj *self = NULL;
    int rv = knitx_get_arg(kstate, 0, &self); KNIT_CRV(rv);
    struct knit_obj *pushed = NULL;
    rv = knitx_get_arg(kstate, 1, &pushed); KNIT_CRV(rv);
    if (self->u.ktype != KNIT_LIST) {
        return knit_error(kstate, KNIT_INVALID_TYPE_ERR, "knitx_str_strip(self, ...) was called with an unexpected type, expecting str");
    }
    struct knit_list *self_l = (struct knit_list *) self;
    rv = knitx_list_push(kstate, self_l, pushed);

    knitx_creturns(kstate, 0);
    return KNIT_OK;
}
int knitxr_input(struct knit *kstate) {
    char buf[256];
    char *p = fgets(buf, 255, stdin);
    if (!p)
        buf[0] = 0;

    struct knit_str *s = NULL;
    int rv = knitx_str_new_strcpy(kstate, &s, buf);
    if (s->len > 0 && s->str[s->len-1] == '\n') {
        s->str[s->len-1] = 0;
        s->len--;
    }
    knitx_stack_rpush(kstate, &kstate->ex.stack, ktobj(s));

    knitx_creturns(kstate, 1);
    return KNIT_OK;
}
int knitxr_str_to_int(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    if (nargs != 1) { 
        return knit_error(kstate, KNIT_NARGS, "knitx_str_to_int(self, ...) was called with a wrong number of arguments, expecting 1 argument");
    }
    struct knit_obj *stro = NULL;
    int rv = knitx_get_arg(kstate, 0, &stro); KNIT_CRV(rv);
    struct knit_str *str = (struct knit_str *)stro;
    
    int n = atoi(str->str);
    struct knit_int *num = NULL;
    rv = knitx_int_new(kstate, &num, n); KNIT_CRV(rv);
    rv = knitx_stack_rpush(kstate, &kstate->ex.stack, ktobj(num));
    knitx_creturns(kstate, 1);
    return KNIT_OK;
}
static int knitxr_print(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    struct knit_str tmp;

    int rv = KNIT_OK;
    rv = knitx_str_init(kstate, &tmp); KNIT_CRV(rv);
    for (int i=0; i<nargs; i++) {
        struct knit_obj *obj = NULL;
        rv = knitx_get_arg(kstate, i, &obj); KNIT_CRV(rv);
        rv = knitx_obj_rep(kstate, obj, &tmp, 1); KNIT_CRV(rv);
        printf("%s", tmp.str);
    }
    printf("\n");
    knitx_creturns(kstate, 0);
    knitx_str_deinit(kstate, &tmp);
    return KNIT_OK;
}
static int knitxr_len(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    if (nargs != 1) { 
        return knit_error(kstate, KNIT_NARGS, "knitxr_len(obj) was called with a wrong number of arguments, expecting 1 argument");
    }
    struct knit_obj *obj = NULL;
    int rv = knitx_get_arg(kstate, 0, &obj); KNIT_CRV(rv);
    struct knit_int *num = NULL;
    rv = knitx_int_new(kstate, &num, 0); KNIT_CRV(rv);

    if (obj->u.ktype == KNIT_LIST) {
        num->value = obj->u.list.len;
    }
    else if (obj->u.ktype == KNIT_STR) {
        num->value = obj->u.str.len;
    }
    else {
        return knit_error(kstate, KNIT_INVALID_TYPE_ERR, "knitx_len(obj) was called with an unexpected type, expecting str or list");
    }

    knitx_stack_rpush(kstate, &kstate->ex.stack, ktobj(num));
    knitx_creturns(kstate, 1);
    return KNIT_OK;
}
static int knitxr_gcwalk(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    if (nargs != 1) { 
        return knit_error(kstate, KNIT_NARGS, "knitxr_len(obj) was called with a wrong number of arguments, expecting 1 argument");
    }
    struct knit_obj *obj = NULL;
    int rv = knitx_get_arg(kstate, 0, &obj); KNIT_CRV(rv);
    struct knit_int *num = NULL;
    rv = knitx_int_new(kstate, &num, 0); KNIT_CRV(rv);

    if (obj->u.ktype == KNIT_LIST) {
        num->value = obj->u.list.len;
    }
    else if (obj->u.ktype == KNIT_STR) {
        num->value = obj->u.str.len;
    }
    else {
        return knit_error(kstate, KNIT_INVALID_TYPE_ERR, "knitx_len(obj) was called with an unexpected type, expecting str or list");
    }

    knitx_stack_rpush(kstate, &kstate->ex.stack, ktobj(num));
    knitx_creturns(kstate, 1);
    return KNIT_OK;
}

const struct knit_builtins kbuiltins = {
    .kstr = {
        .strip = {
            .ktype = KNIT_CFUNC,
            .fptr = knitx_str_strip,
        }
    },
    .klist = {
        .append = {
            .ktype = KNIT_CFUNC,
            .fptr = knitx_list_append,
        }
    },
    .funcs = {
        .print = {
            .ktype = KNIT_CFUNC,
            .fptr = knitxr_print,
        },
        .len = {
            .ktype = KNIT_CFUNC,
            .fptr = knitxr_len,
        },
        .str_to_int = {
            .ktype = KNIT_CFUNC,
            .fptr = knitxr_str_to_int
        },
        .input = {
            .ktype = KNIT_CFUNC,
            .fptr = knitxr_input,
        }
        .gcwalk = {
            .ktype = KNIT_CFUNC,
            .fptr = knitxr_gcwalk,
        }
    }
};

static int knitx_register_cfunction(struct knit *kstate, const char *funcname, knit_func_type cfunc) {
    void *p = NULL;
    int rv = knitx_tmalloc(kstate, sizeof(struct knit_cfunc), &p); KNIT_CRV(rv);
    struct knit_cfunc *func = p;
    func->ktype = KNIT_CFUNC;
    func->fptr = cfunc;
    struct knit_str funcname_str;
    rv = knitx_str_init_const_str(kstate, &funcname_str, funcname); KNIT_CRV(rv);
    rv = knitx_do_global_assign(kstate, &funcname_str, ktobj(func));
    return rv;
}

static int knitx_register_constcfunction(struct knit *kstate, const char *funcname, const struct knit_cfunc *func) {
    struct knit_str funcname_str;
    int rv = knitx_str_init_const_str(kstate, &funcname_str, funcname); KNIT_CRV(rv);
    rv = knitx_do_global_assign(kstate, &funcname_str, ktobj(func)); KNIT_CRV(rv);
    return KNIT_OK;
}

static int knitxr_register_stdlib(struct knit *kstate) {
    int rv = knitx_register_constcfunction(kstate, "print", &kbuiltins.funcs.print); KNIT_CRV(rv);
    rv = knitx_register_constcfunction(kstate, "str_to_int", &kbuiltins.funcs.str_to_int); KNIT_CRV(rv);
    rv = knitx_register_constcfunction(kstate, "input", &kbuiltins.funcs.input); KNIT_CRV(rv);
    rv = knitx_register_constcfunction(kstate, "len", &kbuiltins.funcs.len); KNIT_CRV(rv);
    rv = knitx_register_constcfunction(kstate, "gcwalk", &kbuiltins.funcs.gcwalk); KNIT_CRV(rv);
    return KNIT_OK;
}

