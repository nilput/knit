
.PHONY: all clean
GEN :=        src/knit_vars_hasht.h src/knit_mem_hasht.h src/kobj_hasht.h src/tok_darray.h src/insns_darray.h
GEN := $(GEN) src/knit_objp_darray.h src/knit_frame_darray.h src/knit_expr_darray.h src/knit_stmt_darray.h src/knit_varname_darray.h 
all: knit test $(GEN)
HASHT_INC := -I hasht/src/ -I hasht/third_party/

src/knit_vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh knit_vars_hasht $@
src/kobj_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh kobj_hasht $@
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
src/knit_stmt_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_stmt_darray 'struct knit_stmt *' $@
src/knit_varname_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_varname_darray 'struct knit_varname' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0 -D KNIT_DEBUG_PRINT
debug: all
opt: CFLAGS := $(CFLAGS) -O2
opt: all
test: src/test.c $(GEN) src/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
knit: src/main.c $(GEN) src/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
src/knit.h: src/kdata.h src/kruntime.h
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null
	rm -f knit   2>/dev/null

