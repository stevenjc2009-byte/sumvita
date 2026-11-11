# Host (PC) tests for the maths engine and the GCSE tools. No Vita code here.
# Run in WSL:  make -C tests        (builds and runs every test, exits non-zero on any failure)
#
# Each tests/test_*.c is its own program linked against build/libsumvita.a (all engine +
# tools sources). An archive only pulls in the objects a test actually uses, so a file
# with unresolved calls into code that does not exist yet never breaks other tests.
# A test program prints "PASS n/m" on its last line and exits 1 if any check failed.
# One test only:  make -C tests T=test_parse

CC      ?= gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Werror -O1 -g -I../src -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS := -fsanitize=address,undefined -lm

# plus the UI's pure string helpers (no Vita calls) that have host tests
ENGINE_SRCS := $(sort $(wildcard ../src/engine/*.c) $(wildcard ../src/tools/*.c)) ../src/ui/note_text.c
TEST_SRCS   := $(if $(T),$(T).c,$(sort $(wildcard test_*.c)))
TEST_BINS   := $(patsubst %.c,build/%,$(TEST_SRCS))
LIB_OBJS    := $(patsubst ../src/%.c,build/obj/%.o,$(ENGINE_SRCS))

all: run

build/obj/%.o: ../src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/libsumvita.a: $(LIB_OBJS)
	@rm -f $@
	ar rcs $@ $(LIB_OBJS)

build/%: %.c testlib.h build/libsumvita.a
	@mkdir -p build
	$(CC) $(CFLAGS) $< build/libsumvita.a $(LDFLAGS) -o $@

run: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do \
	  out=$$(./$$t 2>&1); rc=$$?; last=$$(printf '%s\n' "$$out" | tail -1); \
	  if [ $$rc -ne 0 ]; then fail=1; printf '%s\n' "$$out" | grep -E 'FAIL|ERROR|runtime error|Sanitizer' | head -40; fi; \
	  echo "$$t: $$last (exit $$rc)"; \
	done; exit $$fail

clean:
	rm -rf build

.PHONY: all run clean
