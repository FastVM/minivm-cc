CC=gcc
OPT=-O3
8OBJS=cpp.o debug.o dict.o gen.o lex.o vector.o parse.o buffer.o map.o \
     error.o path.o file.o set.o encoding.o
VOBJS=asm.o gc.o jump.o reguse.o int/run.o int/comp.o

REAL_OPT=-fno-ssa-phiopt $(OPT)

default: all

all: bin/minivm-cc bin/minivm-asm

bin/minivm-cc: src/8cc.h main.o $(8OBJS) $(VOBJS)
	@mkdir -p bin
	$(CC) $(REAL_OPT) -o $(@) obj/cc/main.o $(8OBJS:%=obj/cc/%) $(VOBJS:%=obj/vm/%) $(LDFLAGS) -lm

bin/minivm-asm: $(VOBJS) obj/vm/main.o
	@mkdir -p bin
	$(CC) $(REAL_OPT) -o $(@) vm/util/main.c $(VOBJS:%=obj/vm/%) -lm

obj/vm/main.o: 
	@mkdir -p obj/vm
	$(CC) $(REAL_OPT) -o obj/vm/main.o -c vm/util/main.c $(CFLAGS)

$(VOBJS): $(@:%.o=vm/vm/%.c) src/8cc.h src/keyword.inc
	@mkdir -p obj/vm/int
	$(CC) $(REAL_OPT) -o obj/vm/$(@) -c $(@:%.o=vm/vm/%.c) $(CFLAGS)

$(8OBJS) main.o: $(@:%.o=src/%.c) src/8cc.h src/keyword.inc
	@mkdir -p obj/cc
	$(CC) $(REAL_OPT) -o obj/cc/$(@) -c $(@:%.o=src/%.c) $(CFLAGS)

clean: .dummy
	rm -r bin obj

.dummy:
