static int knitx_register_cfunction(struct knit *kstate, const char *funcname, knit_func_type cfunc) {
    void *p = NULL;
    int rv = knitx_tmalloc(kstate, sizeof(struct knit_cfunc), &p); KNIT_CRV(rv);
    struct knit_cfunc *func = p;
    func->ktype = KNIT_CFUNC;
    func->fptr = cfunc;
    struct knit_str funcname_str;
    rv = knitx_str_init_const_str(kstate, &funcname_str, funcname); /*ml*/KNIT_CRV(rv);
    rv = knitx_do_global_assign(kstate, &funcname_str, ktobj(func));
    return rv;
}

static int knitxr_print(struct knit *kstate) {
    int nargs = knitx_nargs(kstate);
    for (int i=0; i<nargs; i++) {
        struct knit_obj *obj = NULL;
        int rv = knitx_get_arg(kstate, i, &obj);
        if (rv != KNIT_OK)
            return rv;
        knitx_obj_dump(kstate, obj);
    }
    knitx_creturns(kstate, 0);
    return KNIT_OK;
}
static int knitxr_register_stdlib(struct knit *kstate) {
    int rv = knitx_register_cfunction(kstate, "print", knitxr_print);
    return rv;
}