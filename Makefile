
.PHONY: all clean
GEN :=        src/vars_hasht.h src/mem_hasht.h src/tok_darray.h src/insns_darray.h
GEN := $(GEN) src/knit_objp_darray.h src/knit_frame_darray.h src/knit_expr_darray.h src/knit_varname_darray.h

all: test $(GEN)
HASHT_INC := -I hasht/src/ -I hasht/third_party/

src/vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh vars_hasht $@
src/mem_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh mem_hasht $@
src/tok_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh tok_darray 'struct knit_tok' $@
src/insns_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh insns_darray 'struct knit_insn' $@
src/knit_objp_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_objp_darray 'struct knit_obj *' $@
src/knit_frame_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_frame_darray 'struct knit_frame' $@
src/knit_expr_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_expr_darray 'struct knit_expr *' $@
src/knit_varname_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_varname_darray 'struct knit_varname' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0
debug: test
opt: CFLAGS := $(CFLAGS) -O2
opt: test
test: src/test.c $(GEN) src/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
src/knit.h: src/kdata.h
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null

