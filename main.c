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

static char *infile;
static char *asmfile;
static char *outfile = NULL;
static bool run = false;
static Buffer *cppdefs;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: 8cc [ -E ][ -a ] [ -h ] <file>\n\n"
            "\n"
            "  -o filename       Output to the specified file\n"
            "  -h                print this help\n"
            "  -r                run the specified file\n"
            "\n");
    exit(exitcode);
}

static char *base(char *path) {
    return basename(strdup(path));
}

static char *replace_suffix(char *filename, char suffix) {
    char *r = format("%s", filename);
    char *p = r + strlen(r) - 1;
    if (*p != 'c')
        error("filename suffix is not .c");
    *p = suffix;
    return r;
}

static FILE *open_asmfile() {
    asmfile = outfile ? outfile : replace_suffix(base(infile), 's');
    if (!strcmp(asmfile, "-"))
        return stdout;
    FILE *fp = fopen(asmfile, "w");
    if (!fp)
        perror("fopen");
    return fp;
}

static void parseopt(int argc, char **argv) {
    cppdefs = make_buffer();
    int i = 1;
    while (i < argc) {
        char *arg = argv[i++];
        if (arg[0] == '-') {
            switch(arg[1]) {
            case 'o':
                outfile = argv[i++];
                // asmfile = argv[i++];
                break;
            case 'r':
                run = true;
                break;
            case 'h':
                usage(0);
                break;
            default:
                usage(1);
            }
        } else if (infile == NULL) {
            infile = arg;
        } else {
            error("only one file at cli is possible");
        }
    }
}

char *get_base_file() {
    return infile;
}

static void preprocess() {
    for (;;) {
        Token *tok = read_token();
        if (tok->kind == TEOF)
            break;
        if (tok->bol)
            printf("\n");
        if (tok->space)
            printf(" ");
        printf("%s", tok2s(tok));
    }
    printf("\n");
    exit(0);
}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    parseopt(argc, argv);
    lex_init(infile);
    cpp_init();
    parse_init();
    set_output_file(open_asmfile());
    if (buf_len(cppdefs) > 0)
        read_from_string(buf_body(cppdefs));

    Vector *toplevels = read_toplevels();
    for (int i = 0; i < vec_len(toplevels); i++) {
        Node *v = vec_get(toplevels, i);
        emit_toplevel(v);
    }

    close_output_file();

    if (run) {
        const char *src = vm_asm_io_read(asmfile);
        vm_asm_buf_t buf = vm_asm(src);
        vm_free((void *)src);
        if (buf.nops == 0) {
            return 1;
        }
        int res = vm_run_arch_int(buf.nops, buf.ops);
        if (res != 0) {
            return 1;
        }
    }
    return 0;
}
