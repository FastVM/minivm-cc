// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "8cc.h"
#define VM_NO_FILE 1
#include "vm/vm/asm.h"

static Vector *infiles = &EMPTY_VECTOR;
static char *asmfile;
static Buffer *cppdefs;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: minivm-cc <file>\n\n"
            "\n"
            "  -h                print this help\n"
            "\n");
    exit(exitcode);
}

static void parseopt(int argc, char **argv) {
    cppdefs = make_buffer();
    int i = 1;
    while (i < argc) {
        char *arg = argv[i++];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case 'h':
                usage(0);
                break;
            default:
                usage(1);
            }
        } else {
            vec_push(infiles, arg);
        }
    }
}

static char *filetype(char *name) {
    int where = strlen(name)-1;
    while (where > 0 && name[where] != '.') {
        where -= 1;
    }
    return &name[where];
}

char *infile;
char *get_base_file(void) {
    return infile;
}

int main(int argc, char **argv) {
    emit_end();
    setbuf(stdout, NULL);
    parseopt(argc, argv);
    Vector *asmbufs = &EMPTY_VECTOR;
    for (int i = 0; i < vec_len(infiles); i++) {
        infile = vec_get(infiles, i);
        char *ext = filetype(infile);
        if (!strcmp(ext, ".c") || !strcmp(ext, ".h") || !strcmp(ext, ".i")) {
            lex_init(infile);
            cpp_init();
            parse_init();
            if (buf_len(cppdefs) > 0)
                read_from_string(buf_body(cppdefs));

            Vector *toplevels = read_toplevels();
            for (int i = 0; i < vec_len(toplevels); i++) {
                Node *v = vec_get(toplevels, i);
                emit_toplevel(v);
            }
        } else if (!strcmp(ext, ".vasm") || !strcmp(ext, ".asm") || !strcmp(ext, ".vs") || !strcmp(ext, ".s")) {
            Buffer *asmbuf = make_buffer();
            FILE *file = fopen(infile, "r");
            while (!feof(file)) {
                char buf[2048];
                int size = fread(buf, sizeof(char), 2048, file);
                buf_printf(asmbuf, "%.*s", size, buf);
            }
            fclose(file);
            vec_push(asmbufs, asmbuf->body);
        }
    }
    Buffer *src = emit_end();
    for (int i = 0; i < vec_len(asmbufs); i++) {
        buf_printf(src, "\n%s\n", vec_get(asmbufs, i));
    }
    vm_asm_buf_t buf = vm_asm(src->body);
    if (buf.nops == 0) {
        return 1;
    }
    int res = vm_run_arch_int(buf.nops, buf.ops);
    if (res != 0) {
        return 1;
    }
    return 0;
}
