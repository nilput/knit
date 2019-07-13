#include "knit.h"

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
    knitx_exec_str(&knit, "result = 'hello {name}!';");
    struct knit_str *str;
    int rv = knitx_get_str(&knit, "name", &str);
    knit_assert_h(rv == KNIT_OK, "get_str() failed");
    knitx_vardump(&knit, "name");
    printf("%s : %s\n", "name", str->str);
    knitx_deinit(&knit);
}
void t3(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "23 + (8 - 5);");
    knitx_deinit(&knit);
}
void (*funcs[])(void) = {
    t1,
    t2,
    t3,
    NULL,
};
int main(int argc, char **argv) {
    int c = 0;
    if (argc > 1)
        c = atoi(argv[1]);
    const int nfuncs = 3;
    if (c <= 0 || c > nfuncs)
        c = 1;
    funcs[c-1]();
}
