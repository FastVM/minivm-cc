// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "8cc.h"
#define VM_NO_FILE 1
#include "../vm/vm/asm.h"

enum {
    OUTPUT_RUN,
    OUTPUT_ASM,
    OUTPUT_BC,
};

static Vector *infiles = &EMPTY_VECTOR;
static char *outfile;
static int outtype = OUTPUT_RUN;
static char *rtsrc = "rt";
static Buffer *cppdefs;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: vmcc <file>\n\n"
            "\n"
            "  -n                dont include runtime\n"
            "  -r                runtime directory\n"
            "  -h                print this help\n"
            "\n");
    exit(exitcode);
}

static char *filetype(char *name) {
    int where = strlen(name)-1;
    while (where > 0 && name[where] != '.') {
        where -= 1;
    }
    return &name[where];
}

static void parseopt(int argc, char **argv) {
    cppdefs = make_buffer();
    int i = 1;
    while (i < argc) {
        char *arg = argv[i++];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case 'n': {
                rtsrc = NULL;
                break;
            }
            case 'r': {
                rtsrc = argv[i++];
                break;
            }
            case 'o': {
                outfile = argv[i++];
                char *ext = filetype(outfile);   
                if (!strcmp(ext, ".vasm")) {
                    outtype = OUTPUT_ASM;
                } else if (!strcmp(ext, ".bc")) {
                    outtype = OUTPUT_BC;
                }
                break;
            }
            case 'h':
                printf("%s\n", arg);
                usage(0);
                break;
            default:
                printf("%s\n", arg);
                usage(1);
            }
        } else {
            vec_push(infiles, arg);
        }
    }
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
    if (rtsrc != NULL) {
        vec_push(infiles, format("%s/src/stdio.c", rtsrc));
        vec_push(infiles, format("%s/src/bitop.c", rtsrc));
        vec_push(infiles, format("%s/src/start.c", rtsrc));
        vec_push(infiles, format("%s/src/tinyalloc.c", rtsrc));
        vec_push(infiles, format("%s/src/mem.vasm", rtsrc));
        add_include_path(format("%s/include", rtsrc));
    }
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
        } else if (!strcmp(ext, ".vasm")) {
            Buffer *asmbuf = make_buffer();
            FILE *file = fopen(infile, "r");
            if (file == NULL) {
                error("no such file: %s\n", infile);
                return 1;
            }
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
    if (outtype == OUTPUT_ASM) {
        FILE *out = fopen(outfile, "w");
        fwrite(src->body, sizeof(char), src->len, out);
        fclose(out);
        return 0;
    }
    // printf("%s\n", src->body);
    vm_asm_buf_t buf = vm_asm(src->body);
    if (buf.nops == 0) {
        return 1;
    }
    if (outtype == OUTPUT_BC) {
        FILE *out = fopen(outfile, "wb");
        fwrite(buf.ops, sizeof(vm_opcode_t), buf.nops, out);
        fclose(out);
        return 0;
    }
    int res = vm_run_arch_int(buf.nops, buf.ops);
    if (res != 0) {
        return 1;
    }
    return 0;
}
