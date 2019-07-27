#include "knit.h"

void interactive(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    char buf[256] = {0};
    char *ln = fgets(buf, 256, stdin);
    while (ln) {
        knitx_exec_str(&knit, buf);
        ln = fgets(buf, 256, stdin);
    }
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}
void t1(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "1 + 3;");
    knitx_deinit(&knit);
}
void t2(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_set_str(&knit, "name", "john"); //short for knitx_init(&knit, ...)
    knitx_exec_str(&knit, "g.result = 'hello {name}!';");
    struct knit_str *str;
    int rv = knitx_get_str(&knit, "name", &str);
    knit_assert_h(rv == KNIT_OK, "get_str() failed");
    knitx_globals_dump(&knit);
    printf("%s : %s\n", "name", str->str);
    knitx_deinit(&knit);
}
void t3(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "g.result = 23 + (8 - 5);");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}
void (*funcs[])(void) = {
    t1,
    t2,
    t3,
};
int main(int argc, char **argv) {
    void (*func)(void) = interactive;
    if (argc > 1) {
        if (argv[1][0] == '-' && argv[1][1] == 'i') {
            func = interactive;
        }
        else {
            int c = atoi(argv[1]) - 1;
            int nfuncs = (sizeof funcs  / sizeof funcs[0]);
            c = c < 0 ? 0 : (c >= nfuncs ? 0 : c);
            func = funcs[c];
        }
    }
    func();
}
