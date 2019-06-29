
.PHONY: all clean
GEN := vars_hasht.h mem_hasht.h tok_darr.h
all: test $(GEN)
HASHT_INC := -I hasht/src/

vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh vars_hasht $@
mem_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh mem_hasht $@
tok_darr.h: darr/src/darr.h
	./darr/scripts/gen_darr.sh tok_darr 'struct katok' $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
debug: CFLAGS := $(CFLAGS) -g3 -O0
debug: test
test: test.c $(GEN) knit.h
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null

