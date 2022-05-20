CFLAGS=-std=c11 -g -I.
OBJS=cpp.o debug.o dict.o gen.o lex.o vector.o parse.o buffer.o map.o \
     error.o path.o file.o set.o encoding.o

default: all

all: 8cc

8cc: 8cc.h main.o $(OBJS)
	$(CC) -o $(@) main.o $(OBJS) $(LDFLAGS)

$(OBJS) main.o: $(@:%.o=%.c) 8cc.h keyword.inc
	$(CC) -o $(@) -c $(@:%.o=%.c) $(CFLAGS)

clean: .dummy
	rm -f 8cc $(OBJS)

.dummy:
