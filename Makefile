
.PHONY: all clean
GEN :=        src/knit/knit_vars_hasht.h src/knit/knit_mem_hasht.h src/knit/kobj_hasht.h src/knit/tok_darray.h src/knit/insns_darray.h
GEN := $(GEN) src/knit/knit_objp_darray.h src/knit/knit_frame_darray.h src/knit/knit_expr_darray.h src/knit/knit_stmt_darray.h src/knit/knit_varname_darray.h 
opt:
all: knit test $(GEN)

src/hasht/src/hasht.h:
	git submodule update --init

src/knit/knit_vars_hasht.h: src/hasht/src/hasht.h
	./src/hasht/scripts/gen_hasht.sh knit_vars_hasht $@
src/knit/kobj_hasht.h: src/hasht/src/hasht.h
	./src/hasht/scripts/gen_hasht.sh kobj_hasht $@
src/knit/tok_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh tok_darray 'struct knit_tok' $@
src/knit/insns_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh insns_darray 'struct knit_insn' $@
src/knit/knit_objp_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh knit_objp_darray 'struct knit_obj *' $@
src/knit/knit_frame_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh knit_frame_darray 'struct knit_frame' $@
src/knit/knit_expr_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh knit_expr_darray 'struct knit_expr *' $@
src/knit/knit_stmt_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh knit_stmt_darray 'struct knit_stmt *' $@
src/knit/knit_varname_darray.h: src/knit/darray/src/darray.h
	./src/knit/darray/scripts/gen_darray.sh knit_varname_darray 'struct knit_varname' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0 -D KNIT_DEBUG_PRINT
debug: all
opt: CFLAGS := $(CFLAGS) -O2
opt: all
san: CFLAGS := $(CFLAGS) -fsanitize=address
san: all
test: src/knit/test.c $(GEN) src/knit/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
knit: src/knit/main.c $(GEN) src/knit/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
src/knit/knit.h: src/knit/kdata.h src/knit/kruntime.h
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null
	rm -f knit   2>/dev/null

