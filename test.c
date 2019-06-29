#include "knit.h"

int main(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_set_str(&knit, "name", "john"); //short for knitx_init(&knit, ...)
    knitx_eval(&knit, "result = 'hello {name}!';");
    struct knit_str *str;
    int rv = knitx_get_str(&knit, "name", &str);
    knit_assert_h(rv == KNIT_OK, "get_str() failed");
    knitx_vardump(&knit, "name");
    printf("%s : %s\n", "name", str->str);
    knitx_deinit(&knit);
}
