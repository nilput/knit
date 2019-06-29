
.PHONY: all clean
GEN := vars_hasht.h mem_hasht.h
all: test vars_hasht.h mem_hasht.h
HASHT_INC := -I hasht/src/

vars_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh vars_hasht $@
mem_hasht.h: hasht/src/hasht.h
	./hasht/scripts/gen_hasht.sh mem_hasht $@
CFLAGS := -Wall -Wextra  -Wno-unused-function -Wno-unused-variable
debug: CFLAGS := $(CFLAGS) -g3 -O0
debug: test
test: test.c $(GEN)
	$(CC) $(CFLAGS) $(HASHT_INC) $< -o $@
clean:
	rm -f $(GEN) 2>/dev/null
	rm -f test   2>/dev/null

