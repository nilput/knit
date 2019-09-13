#include <emscripten.h>
#include <stdio.h>
#include "knit.h"


struct knit knit;

#define BBUFFSZ 4096
char buf[BBUFFSZ] = {0};
int at = 0;
int waiting = 0;
int len = 0;
int semi = 0;

static void idie(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


void interact() {
    //accumulate into buffer, while trying to be smart about language constructs
    char *d = buf + at;
again:
    for (; at<len; at++) {
        switch (buf[at]) {
            case '{': semi = 0; waiting++; break;
            case '}': semi = 0; waiting--; break;
            case '(': semi = 0; waiting++; break;
            case ')': semi = 0; waiting--; break;
            case '[': semi = 0; waiting++; break;
            case ']': semi = 0; waiting--; break;
            case ';': semi++; break;
            case '\n': printf("#> "); break;
        }
        if (!waiting && semi) {
            {
                int tmp = buf[at+1];
                buf[at+1] = 0;
//#define DBG
#ifdef DBG
                printf("buff:\n'''%s'''\n", buf);
                puts("");
                fflush(stdout);
#endif
                knitx_exec_str(&knit, buf);
                buf[at+1] = tmp;
                fflush(stdout);
            }
            len = len - at - 1;
            memmove(buf, buf + at + 1, len); 
            buf[len] = 0;
            at = 0;
            semi = 0;
            goto again;
        }
    }
}

void loop() {
    interact();
}


void EMSCRIPTEN_KEEPALIVE feed_input(char *input) {
    int canread = BBUFFSZ - len - 1;
    int slen = strlen(input);
    if (canread <= 1)
        idie("input buffer full");
    if (slen > canread)
        idie("input buffer full");
    memcpy(buf + len, input, slen);
    len += slen;
    buf[len] = 0;
}

void on_init() {
    int rv = knitx_init(&knit, KNIT_POLICY_EXIT);
    if (rv != KNIT_OK) idie("init failed");
    rv = knitxr_register_stdlib(&knit);
    if (rv != KNIT_OK) idie("init failed");
    emscripten_set_main_loop(loop, 0, 0);
}
void on_end() {

    knitx_deinit(&knit);
}

int main() {

    on_init();
    printf("Knit Interactive mode\n");

    char buf[250];
    char *d = fgets(buf, 249, stdin);
    return 0;
}
