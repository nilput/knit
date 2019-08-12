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

void t6(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    knitx_exec_str(&knit, "g.booltest = function() {\n"
                          "    g.print('2 == 2: ', 2 == 2);\n"
                          "    g.print('2 != 2: ', 2 != 2);\n"
                          "    g.print('2 == 3: ', 2 == 3);\n"
                          "    g.print('2 != 3: ', 2 != 3);\n"
                          "    g.print('2 >= 2: ', 2 >= 2);\n"
                          "    g.print('2 > 2: ',  2 >  2);\n"
                          "    g.print('2 < 2: ',  2 <  2);\n"
                          "    g.print('2 <= 2: ', 2 <= 2);\n"
                          "    g.print('2 > 3: ',  2 > 3);\n"
                          "    g.print('2 >= 3: ',  2 >= 3);\n"
                          "    g.print('2 < 3: ',  2 < 3);\n"
                          "    g.print('2 <= 3: ',  2 <= 3);\n"
                          "};\n"
                          "g.booltest();\n");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}
void t7(void) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
                          
    knitx_exec_str(&knit,
                          "g.count = 0;\n"
                          "g.cant = function() {\n"
                          "    g.print('failed!, shouldnt be executed');\n"
                          "    return true;\n"
                          "};\n"
                          "g.must = function() {\n"
                          "    g.count = g.count + 1;\n"
                          "    return true;\n"
                          "};\n"
                          "g.short = function() {\n"

                          "    g.print('g.r = true   and g.must();');\n"
                          "    g.r = true   and g.must();\n"
                          "    g.print('g.r = ', g.r);\n"

                          "    g.print('g.r = false  and g.cant();');\n"
                          "    g.r = false  and g.cant();\n"
                          "    g.print('g.r = ', g.r);\n"

                          "    g.print('g.r = (false and g.cant()) or g.must();');\n"
                          "    g.r = (false and g.cant()) or g.must();\n"
                          "    g.print('g.r = ', g.r);\n"

                          "    g.print('g.r = (false or g.must()) and g.must();');\n"
                          "    g.r = (false or g.must()) and g.must();\n"
                          "    g.print('g.r = ', g.r);\n"

                          "    g.print('expect count to be 4: ', g.count);\n"
                          "};\n"
                          "g.short();\n");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}

void t8(void) {

    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
                          
    //a_global should be defined as a global because it is at file scope
    knitx_exec_str(&knit,
                          "a_global = 10;\n"
                          "g.count = 0;\n"
                          "g.func = function(an_arg) {"
                          "  a_local = 20;"
                          "  print('a_global: ', a_global);"
                          "  print('a_local:  ', a_local);"
                          "  print('an_arg:   ', an_arg);"
                          "  print('count:    ', count);"
                          "};"
                          "g.print('expecting count to be 0');"
                          "g.func(30);"
                          "g.count = 3;"
                          "g.print('expecting count to be 3');"
                          "g.func(40);"
                          );
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}

void (*funcs[])(void) = {
    t1,
    t2,
    t3,
    t4,
    t5,
    t6,
    t7,
    t8,
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
            if (c < 0 || c >= nfuncs)
                idie("invalid test number specified");
            func = funcs[c];
        }
    }
    func();
}
