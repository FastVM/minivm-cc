CC?=gcc
OPT?=-O3
8OBJS=cpp.o debug.o dict.o gen.o lex.o vector.o parse.o buffer.o map.o \
     error.o path.o file.o set.o encoding.o

REAL_OPT=$(OPT)

default: all

all: bin/minivm-cc

bin/minivm-cc: src/8cc.h main.o $(8OBJS) vm/bin/libminivm.a
	@mkdir -p bin
	$(CC) $(REAL_OPT) -o $(@) obj/cc/main.o $(8OBJS:%=obj/cc/%) vm/bin/libminivm.a $(LDFLAGS) -lm $(FLAGS)

vm/bin/libminivm.a: vm/vm
	$(MAKE) -C vm OPT='$(OPT)' CC='$(CC)' CFLAGS='$(CFLAGS)' bin/libminivm.a

$(8OBJS) main.o: $(@:%.o=src/%.c) src/8cc.h src/keyword.inc
	@mkdir -p obj/cc
	$(CC) -c $(REAL_OPT) -o obj/cc/$(@) $(@:%.o=src/%.c) $(CFLAGS) $(FLAGS)

clean: .dummy
	rm -r bin obj

.dummy:
