
.PHONY: all clean
GEN := vars_hasht.h mem_hasht.h tok_darr.h insns_darr.h knit_objp_darr.h knit_frame_darr.h knit_expr_darr.h
all: test $(GEN)
HASHT_INC := -I hasht/src/

vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh vars_hasht $@
mem_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh mem_hasht $@
tok_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh tok_darr 'struct knit_tok' $@
insns_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh insns_darr 'struct knit_insn' $@
knit_objp_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh knit_objp_darr 'struct knit_obj *' $@
knit_frame_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh knit_frame_darr 'struct knit_frame' $@
knit_expr_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh knit_expr_darr 'struct knit_expr *' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0
debug: test
opt: CFLAGS := $(CFLAGS) -O2
opt: test
test: test.c $(GEN) knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
knit.h: kdata.h
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null

