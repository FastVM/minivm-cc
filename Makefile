CC=gcc
OPT=-O3
8OBJS=cpp.o debug.o dict.o gen.o lex.o vector.o parse.o buffer.o map.o \
     error.o path.o file.o set.o encoding.o
VOBJS=asm.o jump.o int/run.o int/comp.o int/gc.o reguse.o ir/build.o ir/toir.o ir/opt/const.o ir/info.o ir/be/js.o ir/be/lua.o

REAL_OPT=$(OPT)

default: all

all: bin/minivm-cc bin/minivm-asm

bin/minivm-cc: src/8cc.h main.o $(8OBJS) $(VOBJS)
	@mkdir -p bin
	$(CC) $(REAL_OPT) -o $(@) obj/cc/main.o $(8OBJS:%=obj/cc/%) $(VOBJS:%=obj/vm/%) $(LDFLAGS) -lm $(FLAGS)

bin/minivm-asm: $(VOBJS) obj/vm/main.o
	@mkdir -p bin
	$(CC) $(REAL_OPT) -o $(@) vm/util/main.c $(VOBJS:%=obj/vm/%) -lm $(FLAGS)

obj/vm/main.o: 
	@mkdir -p obj/vm
	$(CC) $(REAL_OPT) -o obj/vm/main.o -c vm/util/main.c $(CFLAGS) $(FLAGS)

$(VOBJS): $(@:%.o=vm/vm/%.c) src/8cc.h src/keyword.inc
	mkdir -p obj/vm/$(dir $(basename $(@)))
	$(CC) -DVM_GC_ALLOC='1<<26' $(REAL_OPT) -o obj/vm/$(@) -c $(@:%.o=vm/vm/%.c) $(CFLAGS) $(FLAGS)

$(8OBJS) main.o: $(@:%.o=src/%.c) src/8cc.h src/keyword.inc
	@mkdir -p obj/cc
	$(CC) $(REAL_OPT) -o obj/cc/$(@) -c $(@:%.o=src/%.c) $(CFLAGS) $(FLAGS)

clean: .dummy
	rm -r bin obj

.dummy:
