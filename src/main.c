// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include "../vm/vm/asm.h"
#include "../vm/vm/ir/be/int3.h"
#include "../vm/vm/ir/toir.h"
#include "8cc.h"

void vm_ir_be_js(FILE *of, size_t nargs, vm_ir_block_t *blocks);

enum {
    OUTPUT_BC,
    OUTPUT_JIT,
    OUTPUT_ASM,
    OUTPUT_JS,
};

static Vector *infiles = &EMPTY_VECTOR;
static char *outfile;
static int outtype = OUTPUT_JIT;
static char *rtsrc = "rt";
static Buffer *cppdefs;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: minivm-cc <file>\n"
            "\n"
            "  -v filename       turn jit on or off\n"
            "  -n                dont include runtime\n"
            "  -r                runtime directory\n"
            "  -h                print this help\n"
            "\n");
    exit(exitcode);
}

static char *filetype(char *name) {
    int where = strlen(name) - 1;
    while (where > 0 && name[where] != '.') {
        where -= 1;
    }
    return &name[where];
}

static int parseopt(int argc, char **argv) {
    cppdefs = make_buffer();
    int i = 1;
    while (i < argc) {
        char *arg = argv[i++];
        if (arg[0] == '-') {
            switch (arg[1]) {
                case 'n': {
                    rtsrc = NULL;
                    break;
                }
                case 'r': {
                    rtsrc = argv[i++];
                    break;
                }
                case 'v': {
                    char *name = argv[i++];
                    FILE *file = fopen(name, "r");
                    while (!feof(file)) {
                        putchar(fgetc(file));
                    }
                    fclose(file);
                    exit(0);
                }
                case 'j': {
                    arg += 2;
                    if (!strcmp(arg, "on")) {
                        outtype = OUTPUT_JIT;
                    } else {
                        fprintf(stderr, "unknown jit option: -j%s\n", arg);
                        usage(1);
                    }
                    break;
                }
                case 'o': {
                    outfile = argv[i++];
                    char *ext = filetype(outfile);
                    if (!strcmp(ext, ".vasm") || !strcmp(outfile, "/dev/stdout") || !strcmp(outfile, "/dev/stderr")) {
                        outtype = OUTPUT_ASM;
                    } else if (!strcmp(ext, ".bc")) {
                        outtype = OUTPUT_BC;
                    } else if (!strcmp(ext, ".js") || !strcmp(ext, ".ts")) {
                        outtype = OUTPUT_JS;
                    } else {
                        fprintf(stderr, "unknown file extension: %s\n", ext);
                        usage(1);
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
    return 0;
}

char *infile;
char *get_base_file(void) {
    return infile;
}

int main(int argc, char **argv) {
    emit_end();
    parseopt(argc, argv);
    Vector *asmbufs = &EMPTY_VECTOR;
    if (rtsrc != NULL) {
        vec_push(infiles, format("%s/src/stdio.c", rtsrc));
        vec_push(infiles, format("%s/src/ctype.c", rtsrc));
        vec_push(infiles, format("%s/src/string.c", rtsrc));
        vec_push(infiles, format("%s/src/ta.c", rtsrc));
        // start.c must be last
        vec_push(infiles, format("%s/src/start.c", rtsrc));
        add_include_path(format("%s/include", rtsrc));
    }
    for (int i = 0; i < vec_len(infiles); i++) {
        infile = vec_get(infiles, i);
        char *ext = filetype(infile);
        if (!strcmp(ext, ".vasm")) {
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
            // } else if (!strcmp(ext, ".c") || !strcmp(ext, ".h") || !strcmp(ext, ".i")) {
        } else if (!strcmp(ext, ".c") || !strcmp(ext, ".h") || !strcmp(ext, ".i") || !strcmp(infile, "/dev/stdin")) {
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
        } else {
            error("unknown file: %s", infile);
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
    vm_bc_buf_t buf = vm_asm(src->body);
    if (buf.nops == 0) {
        return 1;
    }
    if (outtype == OUTPUT_BC) {
        FILE *out = fopen(outfile, "wb");
        fwrite(buf.ops, sizeof(vm_opcode_t), buf.nops, out);
        fclose(out);
        return 0;
    }
    vm_ir_block_t *blocks = vm_ir_parse(buf.nops, buf.ops);
    if (outtype == OUTPUT_JS) {
        FILE *out = fopen(outfile, "wb");
        vm_ir_be_js(out, buf.nops, blocks);
        fclose(out);
    } else {
        vm_run_arch_int(buf.nops, buf.ops, NULL);
    }
    return 0;
}
