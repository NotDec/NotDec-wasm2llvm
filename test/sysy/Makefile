.PHONY: all tests clean wat wasm ll elf tests

# default target
all: tests

CLANG=clang-14
WASI_CLANG=/opt/wasi-sdk-20.0/bin/clang

# current dir should be test/sysy, and remember to create out_functional folder
current_dir = $(shell pwd)
BINARY := $(current_dir)/../../build/bin/notdec-wasm2llvm

# targets
FUNCTIONALDIR = functional
SOURCES = $(basename $(wildcard $(FUNCTIONALDIR)/*.c))
# eg: functional => out_functional
PREFIXEDS = $(addprefix out_, $(SOURCES))
wat: $(addsuffix .wat, $(PREFIXEDS))
wasm: $(addsuffix .wasm, $(PREFIXEDS))
ll: $(addsuffix .ll, $(PREFIXEDS))
elf: $(addsuffix .elf, $(PREFIXEDS))
tests: $(addsuffix .test-pass, $(PREFIXEDS))

sylib.ll: sylib.c
	clang-14 -opaque-pointers=0 -S -emit-llvm $< -O1 -o $@

out_$(FUNCTIONALDIR)/%.wasm: $(FUNCTIONALDIR)/%.c
	$(WASI_CLANG) -fno-builtin -fno-lto -Wl,--lto-O0 -I. -g -O0 --no-standard-libraries -Wl,--entry=main -lc -Wl,--allow-undefined -o ./$@ ./$<

%.ll: %.wasm %.wat
	$(BINARY) --force-export-name $< -o $@

%.wat: %.wasm
	wasm2wat -f --enable-all ./$< > ./$@

%.elf: %.ll sylib.ll
	$(CLANG) $< sylib.ll -g -o ./$@

%.test-pass: %.elf
	python3 test_runner.py ./$< $(<:out_%.elf=%.out) $(<:out_%.elf=%.in) ./$@

clean:
	rm out_functional/*
