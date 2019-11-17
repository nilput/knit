MKDIR_P := mkdir -p
.PHONY: all clean
GEN :=        src/generated/knit_vars_hasht.h src/generated/knit_mem_hasht.h src/generated/kobj_hasht.h
GEN := $(GEN) src/generated/tok_darray.h src/generated/insns_darray.h src/generated/knit_varname_darray.h 
GEN := $(GEN) src/generated/knit_objp_darray.h src/generated/knit_frame_darray.h src/generated/knit_expr_darray.h 
GEN := $(GEN) src/generated/knit_stmt_darray.h 

all: test

HASHT_INC := -I hasht/src/ -I hasht/third_party/
$(info $(shell mkdir -p src/generated))
src/generated/knit_vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh knit_vars_hasht $@
src/generated/knit_mem_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh knit_mem_hasht $@
src/generated/kobj_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh kobj_hasht $@
src/generated/tok_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh tok_darray 'struct knit_tok' $@
src/generated/insns_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh insns_darray 'struct knit_insn' $@
src/generated/knit_objp_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_objp_darray 'struct knit_obj *' $@
src/generated/knit_frame_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_frame_darray 'struct knit_frame' $@
src/generated/knit_expr_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_expr_darray 'struct knit_expr *' $@
src/generated/knit_stmt_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_stmt_darray 'struct knit_stmt *' $@
src/generated/knit_varname_darray.h: src/darray/src/darray.h
	./src/darray/scripts/gen_darray.sh knit_varname_darray 'struct knit_varname' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0 -D KNIT_DEBUG_PRINT
debug: test
opt: CFLAGS := $(CFLAGS) -O2
opt: test
src/knit.h: src/kdata.h src/kruntime.h src/knit_common.h src/knit_obj.h src/knit_gc.h
test: src/test.c $(GEN) src/knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@

clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null

