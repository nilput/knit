#include <emscripten.h>
#include <stdio.h>
#include "knit.h"


struct knit knit;

#define BBUFFSZ 4096
char buf[BBUFFSZ] = {0};
int len = 0;

static void idie(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


void interact() {
    //accumulate into buffer, while trying to be smart about language constructs
    int can_exec_to = knit_can_exec(buf, len);
    if (can_exec_to) {
            {
                int tmp = buf[can_exec_to+1];
                buf[can_exec_to+1] = 0;
//#define DBG
#ifdef DBG
                printf("buff:\n'''%s'''\n", buf);
                puts("");
                fflush(stdout);
#endif
                knitx_exec_str(&knit, buf);
                buf[can_exec_to+1] = tmp;
                fflush(stdout);
            }
            len = len - can_exec_to;
            memmove(buf, buf + can_exec_to, len); 
            buf[len] = 0;
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
