#include "knit.h"

void idie(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}
void interactive(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
#define BBUFLEN 4096
    char buf[BBUFLEN] = {0};
    enum foci {
        CURLY,
        PAREN,
        BRACKET,
    };
    int focus[3] = {0, 0, 0};
    int at = 0;
    while (1) {
        int canread = BBUFLEN - at;
        if (canread <= 1)
            idie("input buffer full");
        //accumulate into buffer, while trying to be smart about language constructs
        char *d = buf + at;
        char *ln = fgets(d, canread, stdin);
        if (!ln)
            break;
        int read_bytes = strlen(d); 
        at += read_bytes;
        int semi = 0;
        for (int i=0; i<read_bytes; i++) {
            switch (d[i]) {
                case '{': semi = 0; focus[CURLY]++;   break;
                case '}': semi = 0; focus[CURLY]--;   break;
                case '(': semi = 0; focus[PAREN]++;   break;
                case ')': semi = 0; focus[PAREN]--;   break;
                case '[': semi = 0; focus[BRACKET]++; break;
                case ']': semi = 0; focus[BRACKET]--; break;
                case ';': semi++; break;
            }
        }
        int waiting = 0;
        for (int i=0; i < (int)(sizeof focus / sizeof focus[0]); i++)
            waiting += focus[i];
        if (!waiting && semi) {
            knitx_exec_str(&knit, buf);
            at = 0;
        }
    }
#undef BBUFLEN 
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}
void t1(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    knitx_exec_str(&knit, "g.print('hello world!', 1, 2, 3);"
                          "g.foo = 'test';"
                          "g.print(g.foo);");
    knitx_globals_dump(&knit);
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
void t4(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "1 + 3;");
    knitx_deinit(&knit);
}

void t5(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    knitx_exec_str(&knit, "g.times_two = function(a) {\n"
                          "    g.print('testing functions, argument recieved:', a);\n"
                          "    a = a * 2;\n"
                          "    g.print('argument * 2:', a);\n"
                          "};\n"
                          "g.times_two(3);\n");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}

void (*funcs[])(void) = {
    t1,
    t2,
    t3,
    t4,
    t5,
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
