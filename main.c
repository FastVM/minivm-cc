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
static Buffer *cppdefs;

static void usage(int exitcode) {
    fprintf(exitcode ? stderr : stdout,
            "Usage: 8cc <file>\n\n"
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
    emit_end();
    setbuf(stdout, NULL);
    parseopt(argc, argv);
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
    char *src = emit_end();
    vm_asm_buf_t buf = vm_asm(src);
    if (buf.nops == 0) {
        return 1;
    }
    int res = vm_run_arch_int(buf.nops, buf.ops);
    if (res != 0) {
        return 1;
    }
    return 0;
}
