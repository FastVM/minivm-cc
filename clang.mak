CLANG=clang
CLNAG_OPT=-
OPT=-O3
8OBJS=cpp.o debug.o dict.o gen.o lex.o vector.o parse.o buffer.o map.o \
     error.o path.o file.o set.o encoding.o
VOBJS=asm.o gc.o jump.o reguse.o int/run.o int/comp.o
OBJS=$(8OBJS) $(VOBJS:%=vm/vm/%)

REAL_OPT=$(CLANG_OPT) $(OPT) 

default: all

all: minivm-cc

minivm-cc: 8cc.h main.o $(OBJS)
	$(CLANG) $(REAL_OPT) -o $(@) main.o $(OBJS) $(LDFLAGS) -lm

$(OBJS) main.o: $(@:%.o=%.c) 8cc.h keyword.inc
	$(CLANG) $(REAL_OPT) -o $(@) -c $(@:%.o=%.c) $(CFLAGS)

clean: .dummy
	rm -f 8cc $(OBJS)

.dummy:
