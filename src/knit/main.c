#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "knit.h"

#if defined(__linux__) || defined(__apple__)
    #include <unistd.h>
    #define KNIT_HAVE_ISATTY
#endif

static struct knopts {
    int verbose;
    int interactive;
    char *infile;
} knopts = {0};

static void help(char *progname) {
    fprintf(stderr, "./%s OPTION [ file ]\n"
            "-f     : input file\n"
            "-v     : verbose\n"
            "-i     : interactive\n"
            "-h     : help\n", progname == NULL ? "knit" : progname);
    exit(0);
}


static void parse_argv(char *argv[], int argc) {
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            knopts.verbose = 1;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            knopts.interactive = 1;
        }
        else if (strcmp(argv[i], "-h") == 0) {
            help(argv[0]);
        }
        else if (strncmp(argv[i], "-f", 2) == 0) {
            if (strlen(argv[i]) > 2) {
                knopts.infile = argv[i] + 2;
            }   
            else if (argc > i + 1) {
                knopts.infile = argv[i+1];
                i++;
            }
        }
        else if (i == argc - 1) {
            knopts.infile = argv[i];
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

void interactive(const char *n) {
    struct knit knit;
    knitx_init(&knit, KNIT_POLICY_EXIT);
    knitxr_register_stdlib(&knit);


#define BBUFFSZ 4096
    char buf[BBUFFSZ] = {0};
    int len = 0;

#ifdef KNIT_HAVE_ISATTY
    int istty = isatty(0);
#else
    int istty = 1;
#endif

    if (istty) {
        printf("Knit Interactive mode v0.1.0\n");
    }

    while (1) {
        int canread = BBUFFSZ - len;
        if (canread <= 1)
            idie("input buffer full");
        //accumulate into buffer, while trying to be smart about language constructs
        char *d = buf + len;
        if (istty) {
            printf("#> ");
        }
        char *line = fgets(d, canread, stdin);
        if (!line) {
            if (len) {
                knitx_exec_str(&knit, buf);
            }
            break;
        }
        int read_bytes = strlen(d); 
        len += read_bytes;
        int can_exec_to = knit_can_exec(buf, len);
        if (can_exec_to) {
            {
                int tmp = buf[can_exec_to];
                buf[can_exec_to] = 0;
                knitx_exec_str(&knit, buf);
                buf[can_exec_to] = tmp;
            }
            len = len - can_exec_to;
            memmove(buf, buf + can_exec_to, len); 
            buf[len] = 0;
        }
    }
#undef BBUFFSZ 

#ifdef KNIT_DEBUG_PRINT
    if (KNIT_DBG_PRINT) {
        knitx_globals_dump(&knit);
    }
#endif
    knitx_deinit(&knit);
}

void read_stdin(const char *n) {
    interactive(n);
}

void exec_file(const char *filename) {
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
int main(int argc, char **argv) {
    void (*func)(const char *) = interactive;
    char testname[255] = {0};
    char *arg = NULL;

    parse_argv(argv, argc);
    if (knopts.verbose)
        KNIT_DBG_PRINT = 1;
    if (knopts.infile != NULL) {
        func = exec_file;
        arg = knopts.infile;
    }
    else if (knopts.interactive) {
        func = interactive;
    }
    else {
        func = read_stdin;
    }
    func(arg);
}
