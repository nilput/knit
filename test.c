#include "kastr.h"

int main(void) {
    struct kastr kastr;
    kastrx_init(&kastr, KASTR_POLICY_EXIT);
    kastrx_set_str(&kastr, "name", "john"); //short for kastrx_init(&kastr, ...)
    kastrx_eval(&kastr, "result = 'hello {name}!';");
    struct kastr_str *str;
    int rv = kastrx_get_str(&kastr, "name", &str);
    kastr_assert_h(rv == KASTR_OK, "get_str() failed");
    kastrx_vardump(&kastr, "name");
    printf("%s : %s\n", "name", str->str);
    kastrx_deinit(&kastr);
}
