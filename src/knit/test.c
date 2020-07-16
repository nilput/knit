#include "knit.h"

/*
 * TODO: turn this into an actual test, by checking output and failing on unexpected output
*/

#if defined(__linux__) || defined(__apple__)
    #include <unistd.h>
    #define KNIT_HAVE_ISATTY
#endif

static struct knopts {
    int all;
    int verbose;
    int testno;
    char *infile;
} knopts = {0};
static void parse_argv(char *argv[], int argc) {
    knopts.all = 1;
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v")==0) {
            knopts.verbose = 1;
        }
        else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
            knopts.all = 0;
            knopts.testno = atoi(argv[i]);
        }
        else {
            fprintf(stderr, "unknown arg: '%s'\n", argv[i]);
        }
    }
}

static void idie(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
char *readordie(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f)
        idie("failed to open '%s'\n", filename);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *m = malloc(len + 1);
    if (!m) 
        idie("failed to allocate a buffer\n");
    long read = fread(m, 1, len, f);
    if (read != len)
        idie("failed to read '%s', partially read %d/%d\n", filename, read, len);
    ((char*)m)[len] = 0;
    return m;
}

void t1(const char *unused) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    knitx_exec_str(&knit, "g.print('hello world! ', 1, 2, 3);"
                          "g.foo = 'test';"
                          "g.print(g.foo);");
#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_globals_dump(&knit);
    }
#endif
    knitx_deinit(&knit);
}
void t2(const char *unused) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_set_str(&knit, "name", "john"); //short for knitx_init(&knit, ...)
    knitx_exec_str(&knit, "g.result = 'hello {name}!';");
    struct knit_str *str = NULL;
    int rv = knitx_get_str(&knit, "name", &str);
    knit_assert_h(rv == KNIT_OK, "get_str() failed");
#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_globals_dump(&knit);
    }
#endif
    printf("%s : %s\n", "name", str->str);
    knitx_deinit(&knit);
}
void t3(const char *unused) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "g.result = 23 + (8 - 5);");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}
void t4(const char *unused) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitx_exec_str(&knit, "1 + 3;");
    knitx_deinit(&knit);
}

void t5(const char *unused) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    knitx_exec_str(&knit, "g.times_two = function(a) {\n"
                          "    g.print('testing functions, argument recieved is: ', a);\n"
                          "    c = a; \n"
                          "    a = a * 2;\n"
                          "    g.print(c, ' * 2 = ',  a);\n"
                          "};\n"
                          "g.times_two(3);\n");
    knitx_globals_dump(&knit);
    knitx_deinit(&knit);
}

void t6(const char *unused) {
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
void t7(const char *unused) {
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

void t8(const char *unused) {

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

void generic_file_test(const char *filename) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);
    char *buf = readordie(filename);
    knitx_exec_str(&knit, buf);
    free(buf);
#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_globals_dump(&knit);
    }
#endif
    knitx_deinit(&knit);
}

void exec_file(const char *a) {
    generic_file_test(knopts.infile);
}

void (*funcs[])(const char *unused) = {
    t1,
    t2,
    t3,
    t4,
    t5,
    t6,
    t7,
    t8,
};

void run_test(int n) {
    void (*func)(const char *) = NULL;
    int nfuncs = sizeof funcs  / sizeof funcs[0];
    char testname[255] = {0};
    char *arg = NULL;
    printf("Running test %d\n", n);
    if (n <= 0 || n > nfuncs) {
        snprintf(testname, sizeof testname, "tests/t%d.kn", n);
        func = generic_file_test;
        arg = testname;
    }
    else {
        func = funcs[n - 1];
    }
    func(arg);
}

int main(int argc, char **argv) {

    parse_argv(argv, argc);
    if (knopts.verbose)
        KNIT_DBG_PRINT = 1;
    if (knopts.all) {
        for (int i=1; i<=26; i++) {
            run_test(i);
        }
    }
    else {
        run_test(knopts.testno);
    }
}
