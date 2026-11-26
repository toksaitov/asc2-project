# Builds the C compiler of Almurut II: TinyCC with a code generator for the
# machine. The TinyCC release is kept as its original archive. The build
# unpacks it, changes the files that all targets share with
# `asc2-target.patch`, and compiles it together with `asc2-gen.c` and
# `asc2-link.c`, which stay next to this file.

CC=gcc
ASA=../asc2-assembler/asa
VERSION=0.9.27
TARBALL=tcc-$(VERSION).tar.bz2
SHA256=de23af78fca90ce32dff2dd45b3432b2334740bb9bb7b05bf60fdbfc396ceb9c

BUILD=build
SRC=$(BUILD)/tcc-$(VERSION)
TCC=$(BUILD)/tcc

# TinyCC looks for its headers and its runtime library in these directories.
# `{B}` stands for the directory in `CONFIG_TCCDIR`.
TCC_DEFS=-DONE_SOURCE=1 -DTCC_TARGET_ASC2 \
         -DCONFIG_TCCDIR='"$(abspath $(BUILD))"' \
         -DCONFIG_TCC_SYSINCLUDEPATHS='"{B}/include"' \
         -DCONFIG_TCC_LIBPATHS='"{B}"' \
         -DCONFIG_TCC_CRTPREFIX='"{B}"'

# TinyCC's own code is not free of warnings under a modern compiler. These
# are the ones that its own build turns off.
TCC_WARNINGS=-Wall -Wno-pointer-sign -Wno-sign-compare -Wno-unused-result -Wno-unused-function -Wno-string-plus-int -Wno-deprecated-declarations

HEADERS=$(BUILD)/include/stdarg.h $(BUILD)/include/stdbool.h $(BUILD)/include/stddef.h

.PHONY: all
all: $(TCC) $(BUILD)/libtcc1.a $(HEADERS)

# The marker file is newer than the archive and the patch when the tree is
# unpacked and patched.
$(SRC)/.patched: $(TARBALL) asc2-target.patch
	rm -rf $(SRC)
	mkdir -p $(BUILD)
	(shasum -a 256 $(TARBALL) 2> /dev/null || sha256sum $(TARBALL)) | grep -q '^$(SHA256) '
	tar -xjf $(TARBALL) -C $(BUILD)
	patch -s -d $(SRC) -p1 < asc2-target.patch
	echo '#define TCC_VERSION "$(VERSION)"' > $(SRC)/config.h
	touch $@

$(TCC): $(SRC)/.patched asc2-gen.c asc2-link.c
	$(CC) -O1 $(TCC_WARNINGS) $(TCC_DEFS) -I . -I $(SRC) -o $@ $(SRC)/tcc.c -lm

# The runtime library holds what the compiler calls for the operations that
# the machine has no instructions for. The new compiler builds it. Each file
# of `lib/` is an object file of its own in the library, and the linker takes
# only those that a program needs.
LIB_OBJS=$(BUILD)/lib/division.o $(BUILD)/lib/llong.o $(BUILD)/lib/memory.o $(BUILD)/lib/memory_asm.o

$(BUILD)/lib/%.o: lib/%.c $(TCC)
	mkdir -p $(BUILD)/lib
	$(TCC) -c -o $@ $<

$(BUILD)/lib/%.o: lib/%.s $(ASA)
	mkdir -p $(BUILD)/lib
	$(ASA) -c $< $@

$(ASA):
	$(MAKE) -C ../asc2-assembler all

$(BUILD)/libtcc1.a: $(LIB_OBJS)
	rm -f $@
	$(TCC) -ar rcs $@ $(LIB_OBJS)

$(BUILD)/include/%.h: $(SRC)/.patched
	mkdir -p $(BUILD)/include
	cp $(SRC)/include/$*.h $@

.PHONY: clean
clean:
	rm -rf $(BUILD)
