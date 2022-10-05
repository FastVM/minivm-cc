/*
 * KLSYS Part 0, Virtual machine and bootstrapping interpreter
 * Nils M Holm, 2019, 2020, 2021
 * In the public domain or distributed under the CC0
 */

#if defined(__MINIVM__)
#define KL16BIT
#define BOOTSTRAP
#endif

#define MAGIC "KS00"

#ifdef KL16BIT
#undef KL24BIT
#else
#define KL24BIT
#endif

#ifdef KL24BIT

#define WORDSIZE 3
#define WORDMASK 0xffffff
#define cell unsigned int
#define byte unsigned char
#define FIXLIM 16777215ul

#define NCELLS 262144ul
#define NVCELLS 262144ul

#else

#define WORDSIZE 2
#define WORDMASK 0xffff
#define cell unsigned short
#define byte unsigned char
#define FIXLIM 65535u

#define NCELLS 65535u
#define NVCELLS 65535u

#endif /* KL24BIT */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MINIVM__)
int write(int fd, const char *buffer, unsigned count) {
    for (unsigned i = 0; i < count; i++) {
        putchar(buffer[i]);
    }
    return count;
}

int read(const int fd, char *const buffer, const unsigned count) {
    for (unsigned i = 0; i < count; i++) {
        buffer[i] = getchar();
        if (buffer[i] == '\n') {
            break;
        }
    }
    return count;
}

int open(const char *name, int oflag) {
    printf("open '%s'\n", name);
    return -1;
}

int remove(const char *name) {
    printf("remove '%s'\n", name);
    return 0;
}

int rename(const char *old, const char *new_) {
    printf("rename '%s' -> '%s'\n", old, new_);
    return 0;
}

int close(int fd) {
    return 0;
}

int creat(const char *filename, int pmode) {
    return 0;
}

void exit(int c) {}
#endif

#define O_RDONLY 1
#define O_WRONLY 2

#define SYMLEN 64
#define BUFLEN 256

#define ENDOFFILE 256

#define NIL 0
#define FRELIS 1
#define FREVEC 2
#define SYMLIS 3
#define GENSYM 4
#define UNDEF 5
#define ERROR 6
#define START 7
#define RESET 8
#define VMGLOB 9
#define TOPSYM 10
#define NULOBJ 11
#define EOTOBJ 12
#define CELLS 13
#define LOWCORE 14

byte Tag[NCELLS];

cell Car[NCELLS],
    Cdr[NCELLS];

cell Vectors[NVCELLS];

#define nullvec() (caar(NULOBJ))
#define nullbve() (cadar(NULOBJ))
#define nullstr() (cddar(NULOBJ))

cell car(cell x) { return Car[x]; }
cell cdr(cell x) { return Cdr[x]; }

void rplaca(cell x, cell v) { Car[x] = v; }
void rplacd(cell x, cell v) { Cdr[x] = v; }

#define MARK 0x80
#define SWAP 0x40
#define ATOM 0x20
#define VECT 0x10
#define TYPE 0x0f /* type mask */

enum types {
    TSYM = 1, /* symbol */
    TFIX,     /* fixnum */
    TFUN,     /* function */
    TCFN,     /* compiled function */
    TBVE,     /* byte vector */
    TVEC,     /* vector */
    TSTR,     /* string */
    TESC      /* escape continuation */
};

#define atomp(n) (NIL == n || (Tag[n] & (ATOM | TSYM)))
#define type(n, t) ((Tag[n] & TYPE) == (t))
#define symbolp(n) (type(n, TSYM))
#define funp(n) (type(n, TFUN))
#define fixp(n) (type(n, TFIX))
#define bytevecp(n) (type(n, TBVE))
#define stringp(n) (type(n, TSTR))
#define vectorp(n) (type(n, TVEC))
#define cfnp(n) (type(n, TCFN))
#define esccontp(n) (type(n, TESC))

#define bytevec(n) ((byte *)&Vectors[Car[n]])
#define string(n) (bytevec(n))
#define vector(n) (&Vectors[Car[n]])
#define veclen(n) (Vectors[Car[n] - 1])
#define veclink(n) (Vectors[Car[n] - 2])
#define vecndx(n) (veclink(n))
#define vecsize(n) (2 + n)
#define nbytes(n) (sizeof(cell) * veclen(n))
#define nchars(n) (nbytes(n))

static int caar(int x) { return Car[Car[x]]; }
static int cadr(int x) { return Car[Cdr[x]]; }
static int cdar(int x) { return Cdr[Car[x]]; }
static int cddr(int x) { return Cdr[Cdr[x]]; }

static int cadar(int x) { return Car[Cdr[Car[x]]]; }
static int caddr(int x) { return Car[Cdr[Cdr[x]]]; }
static int cddar(int x) { return Cdr[Cdr[Car[x]]]; }

static int cadddr(int x) { return Car[Cdr[Cdr[Cdr[x]]]]; }
static int cddddr(int x) { return Cdr[Cdr[Cdr[Cdr[x]]]]; }

#ifdef BOOTSTRAP
static int caadr(int x) { return Car[Car[Cdr[x]]]; }
static int cdadr(int x) { return Cdr[Car[Cdr[x]]]; }
static int caddar(int x) { return Car[Cdr[Cdr[Car[x]]]]; }
static int cdddar(int x) { return Cdr[Cdr[Cdr[Car[x]]]]; }
#endif /* BOOTSTRAP */

cell Inbuf, Outbuf;
int Infd, Inptr, Inlim, Rejected;
int Outfd, Outptr, Outlim;

static void flush(void) {
    if (0 == Outfd) return;
    write(Outfd, bytevec(Outbuf), Outptr);
    Outptr = 0;
}

static void error(char *m, cell n);

static void blockwrite(char *s, int k) {
    int n;

    if (Outptr + k >= BUFLEN && Outptr > 0) {
        n = BUFLEN - Outptr;
        memcpy(bytevec(Outbuf) + Outptr, s, n);
        if (write(Outfd, bytevec(Outbuf), BUFLEN) < BUFLEN)
            error("write error", UNDEF);
        k -= n;
        s += n;
        Outptr = 0;
    }
    while (k >= BUFLEN) {
        if (write(Outfd, s, BUFLEN) < BUFLEN)
            error("write error", UNDEF);
        k -= BUFLEN;
        s += BUFLEN;
    }
    memcpy(bytevec(Outbuf) + Outptr, s, k);
    Outptr += k;
    if (1 == Outfd || 2 == Outfd) flush();
}

static void pr(char *s) {
    blockwrite(s, strlen(s));
}

static void nl(void) {
    pr("\n");
    flush();
}

static char *ntoa(cell n);

static void prnum(int n) { pr(ntoa(n)); }

volatile int Error;

char *Error_msg;
cell Error_obj;
int VMrun;

#ifdef BOOTSTRAP
static void prinm(cell n);
static void resetfiles(void);
#endif /* BOOTSTRAP */

static void error(char *m, cell n) {
#ifndef BOOTSTRAP
    Error_msg = m;
    Error_obj = n;
#else
    resetfiles();
    Infd = 0;
    Inptr = Inlim = 0;
    Rejected = 255;
    pr("boot? ");
    pr(m);
    if (n != UNDEF) {
        pr(": ");
        prinm(n);
    }
    nl();
#endif /* !BOOTSTRAP */
    __builtin_trap();
}

static void fatal(char *m) {
    error(m, UNDEF);
    pr("boot? aborting");
    nl();
    flush();
    exit(1);
}

static char *ntoa(cell n) {
    static char buf[20];
    char *q, *p;

    p = q = &buf[sizeof(buf) - 1];
    *p = 0;
    while (n || p == q) {
        p--;
        *p = n % 10 + '0';
        n = n / 10;
    }
    return p;
}

cell Acc, Env;

cell Stack, Mstack;
cell Istack, Ostack;

cell VMenv, VMargs;
cell VMprog, VMip;

cell Tmp, Tmpcar, Tmpcdr;

int Verbose_GC, Debug_GC;

#ifdef KL24BIT

#define word(b, i) (b[i] | (b[i + 1] << 8) | (b[i + 2] << 16))

void putword(byte *b, cell i, cell w) {
    b[i] = w & 255;
    b[i + 1] = (w >> 8) & 255;
    b[i + 2] = (w >> 16) & 255;
}

#else

#define word(b, i) (b[i] | (b[i + 1] << 8))

void putword(byte *b, cell i, cell w) {
    b[i] = w & 255;
    b[i + 1] = (w >> 8) & 255;
}

#endif /* KL24BIT */

/* Deutsch/Schorr/Waite graph marker */

static void mark(cell n) {
    cell p, x, *v, i;
    byte *b;

    p = NIL;
    for (;;) {
        if (Tag[n] & MARK) {
            if (NIL == p) break;
            if ((Tag[p] & TYPE) == TVEC) {
                i = vecndx(p);
                v = vector(p);
                if (Tag[p] & SWAP && i + 1 < veclen(p)) {
                    x = v[i + 1];
                    v[i + 1] = v[i];
                    v[i] = n;
                    n = x;
                    vecndx(p) = i + 1;
                } else {
                    Tag[p] &= ~SWAP;
                    x = p;
                    p = v[i];
                    v[i] = n;
                    n = x;
                    veclink(n) = n;
                }
            } else if (Tag[p] & SWAP) {
                Tag[p] &= ~SWAP;
                x = cdr(p);
                rplacd(p, car(p));
                rplaca(p, n);
                n = x;
            } else {
                x = p;
                p = cdr(x);
                rplacd(x, n);
                n = x;
            }
        } else if ((Tag[n] & TYPE) == TVEC) {
            Tag[n] |= MARK;
            if (veclen(n) != 0) {
                Tag[n] |= SWAP;
                vecndx(n) = 0;
                v = vector(n);
                x = v[0];
                v[0] = p;
                p = n;
                n = x;
            } else {
                veclink(n) = n;
            }
        } else if (Tag[n] & ATOM) {
            Tag[n] |= MARK;
            if (Tag[n] & VECT) veclink(n) = n;
            if ((Tag[n] & TYPE) == TCFN) {
                b = bytevec(cadr(n));
                mark(word(b, 1));
            }
            x = cdr(n);
            rplacd(n, p);
            p = n;
            n = x;
        } else {
            Tag[n] |= (MARK | SWAP);
            x = car(n);
            rplaca(n, p);
            p = n;
            n = x;
        }
    }
}

int n = 0;

static cell gc() {
    cell i, k, fre;

    k = 0;
    mark(Acc);
    mark(Env);
    mark(Stack);
    mark(Mstack);
    mark(VMprog);
    mark(VMenv);
    mark(VMargs);
    mark(car(SYMLIS));
    mark(car(GENSYM));
    mark(car(START));
    mark(car(ERROR));
    mark(car(RESET));
    mark(car(VMGLOB));
    mark(car(TOPSYM));
    mark(car(NULOBJ));
    mark(car(CELLS));
    mark(Istack);
    mark(Ostack);
    mark(Inbuf);
    mark(Outbuf);
    mark(Tmpcar);
    mark(Tmpcdr);
    mark(Tmp);
    mark(Error_obj);
    fre = NIL;
    for (i = LOWCORE; i < NCELLS; i++) {
        if (0 == (Tag[i] & MARK)) {
            rplacd(i, fre);
            fre = i;
            k++;
        } else {
            Tag[i] &= ~MARK;
        }
    }
    rplaca(FRELIS, fre);
    if (Verbose_GC) {
        prnum(k);
        pr(" nodes reclaimed");
        nl();
    }
    return k;
}

static void restcont(cell x) {
    VMip = car(x);
    VMprog = cadr(x);
    VMenv = caddr(x);
    VMargs = cadddr(x);
    Stack = cddddr(x);
}

static void critical(char *m) {
    pr("\n? critical error: ");
    pr(m);
    restcont(car(RESET));
    __builtin_trap();
}

static cell cons3(cell a, cell d, cell t) {
    cell n;

    if (Debug_GC || NIL == car(FRELIS)) {
        Tmpcdr = d;
        if (0 == t || TSYM == t) Tmpcar = a;
        gc();
        Tmpcar = Tmpcdr = NIL;
        if (NIL == car(FRELIS)) critical("out of nodes");
    }
    n = car(FRELIS);
    rplaca(FRELIS, cdar(FRELIS));
    rplaca(n, a);
    rplacd(n, d);
    Tag[n] = t;
    return n;
}

#define cons(a, d) (cons3(a, d, 0))

#define VECLINK 0
#define VECSIZE 1
#define VECDATA 2

static void unmark_vecs(void) {
    cell p, k, link;

    p = 0;
    while (p < car(FREVEC)) {
        link = p;
        k = Vectors[p + VECSIZE];
        p += vecsize(k);
        Vectors[link] = NIL;
    }
}

static cell gcv(cell *nc) {
    cell v, k, to, from;

    unmark_vecs();
    k = gc(); /* re-mark live vectors */
    if (nc) *nc = k;
    to = from = 0;
    while (from < car(FREVEC)) {
        v = Vectors[from + VECSIZE];
        k = vecsize(v);
        if (Vectors[from + VECLINK] != NIL) {
            if (to != from) {
                memmove(&Vectors[to], &Vectors[from],
                        k * sizeof(cell));
                rplaca(Vectors[to + VECLINK], to + VECDATA);
            }
            to += k;
        }
        from += k;
    }
    rplaca(FREVEC, to);
    k = NVCELLS - to;
    if (Verbose_GC) {
        prnum(k);
        pr(" cells reclaimed");
        nl();
    }
    return k;
}

static cell newvec(cell type, cell size, int clear) {
    cell n, v, w;

    Tmp = cons(NIL, NIL);
    w = vecsize(size);
    if (car(FREVEC) + w >= NVCELLS) {
        gcv(NULL);
        if (car(FREVEC) + w >= NVCELLS)
            critical("out of vector space");
    }
    v = car(FREVEC);
    rplaca(FREVEC, car(FREVEC) + w);
    n = Tmp;
    Tmp = NIL;
    Car[n] = v + VECDATA;
    Tag[n] = ATOM | VECT | type;
    Vectors[v + VECLINK] = n;
    Vectors[v + VECSIZE] = size;
    /* assert NIL == 0 */
    if (clear) memset(&Vectors[v + VECDATA], 0, size * sizeof(cell));
    return n;
}

static cell newbytevec(cell n, cell v) {
    cell b;

    if (0 == n) return nullbve();
    b = newvec(TBVE, (n + sizeof(cell) - 1) / sizeof(cell), 0);
    memset(bytevec(b), v, nbytes(b));
    return b;
}

static cell newstr(cell n, cell v) {
    cell s;

    if (0 == n) return nullstr();
    s = newvec(TSTR, (n + sizeof(cell) - 1) / sizeof(cell), 0);
    memset(string(s), 128, nchars(s));
    memset(string(s), v, n);
    return s;
}

static cell nrev(cell n) {
    cell m, h;

    m = NIL;
    while (n != NIL) {
        h = cdr(n);
        rplacd(n, m);
        m = n;
        n = h;
    }
    return m;
}

#define save(n) (Stack = cons(n, Stack))

static cell unsave(cell k) {
    cell n;

    while (k) {
        if (NIL == Stack) fatal("stack empty");
        n = car(Stack);
        Stack = cdr(Stack);
        k--;
    }
    return n;
}

static cell strsym(char *s) {
    cell i, n;

    i = 0;
    if (0 == s[i]) return NIL;
    save(n = cons3(NIL, NIL, ATOM));
    for (;;) {
        rplaca(n, (s[i] << 8) | s[i + 1]);
        if (0 == s[i + 1] || 0 == s[i + 2]) break;
        rplacd(n, cons3(NIL, NIL, ATOM));
        n = cdr(n);
        i += 2;
    }
    n = unsave(1);
    return cons3(n, UNDEF, TSYM);
}

static char *symstr(cell n) {
    static char b[SYMLEN + 2];
    cell i;

    i = 0;
    n = car(n);
    while (n != NIL) {
        b[i] = car(n) >> 8;
        b[i + 1] = car(n) & 0xff;
        i += 2;
        n = cdr(n);
    }
    b[i] = 0;
    return b;
}

static cell stringlen(cell n) {
    cell k;

    k = nbytes(n);
    while (k && string(n)[k - 1] > 127)
        k--;
    return k;
}

static char *stringval(cell x) {
    static char b[BUFLEN * 2 + 1];
    cell k;

    k = stringlen(x);
    strncpy(b, (char *)string(x), BUFLEN * 2);
    b[BUFLEN * 2] = 0;
    if (k < BUFLEN * 2) b[k] = 0;
    return b;
}

#define mkfix(n) (cons3(n, NIL, ATOM | TFIX))

#define fixval(n) (car(n))

cell S_eq;

static cell mkht(void) {
    cell n;

    n = mkfix(0);
    save(n);
    n = cons(n, cons(S_eq, newvec(TVEC, 997, 1)));
    unsave(1);
    return n;
}

#define htcount(d) (car(d))
#define httest(d) (cadr(d))
#define htdata(d) (cddr(d))
#define htlen(d) (veclen(htdata(d)))
#define htslots(d) (vector(htdata(d)))

static cell hash(cell x, cell k) {
    cell h = 0x2345;
    char *s;

    s = symstr(x);
    while (*s) h = (((h << 5) & 0x7fff) + h ^ *s++) & 0x7fff;
    return h % k;
}

static cell htlookup(cell d, cell k) {
    cell h, x;
    char s[SYMLEN + 1];

    h = hash(k, htlen(d));
    x = htslots(d)[h];
    strncpy(s, symstr(k), SYMLEN);
    s[SYMLEN] = 0;
    while (x != NIL) {
        if (strcmp(symstr(caar(x)), s) == 0)
            return caar(x);
        x = cdr(x);
    }
    return NIL;
}

static void htadd(cell d, cell k) {
    cell e;
    int h;

    h = hash(k, htlen(d));
    e = cons(cons(k, NIL), htslots(d)[h]);
    htslots(d)[h] = e;
    rplaca(d, mkfix(fixval(car(d)) + 1));
}

#define findsym(k) htlookup(car(SYMLIS), k)

#define SELF FIXLIM

static cell addsym(char *s, cell v) {
    cell n, y;

    save(SELF == v ? NIL : v);
    n = strsym(s);
    y = findsym(n);
    if (y != NIL) {
        unsave(1);
        return y;
    }
    save(n);
    if (SELF == v)
        rplacd(n, cons(n, NIL));
    else
        rplacd(n, cons(v, NIL));
    htadd(car(SYMLIS), n);
    unsave(2);
    return n;
}

cell S_apply, S_if, S_ifnot, S_lambda, S_progn, S_quasiquote,
    S_quote, S_setq, S_unquote, S_unquote_splice;

cell S_t, S_dot, S_rparen;

cell S_add1, S_append_file, S_atom, S_bytevecp, S_car, S_cdr, S_cons,
    S_kl_crval, S_define, S_delete_file, S_difference, S_divide,
    S_eq, S_eqv, S_end_of_file_p, S_error, S_esccontp,
    S_finish_output, S_fixnump, S_functionp, S_gc, S_greaterp,
    S_halt, S_kl_indcr, S_input_file, S_kl_car, S_kl_cdr,
    S_kl_cons3, S_kl_glob_env, S_kl_rplacd, S_leftshift, S_lessp,
    S_load, S_logand, S_lognot, S_logor, S_logxor, S_make_bytevec,
    S_make_string, S_make_vector, S_maksym, S_not_greaterp,
    S_not_lessp, S_output_file, S_peek_byte, S_plus, S_prin1,
    S_princ, S_read, S_read_byte, S_rem, S_rightshift, S_rplaca,
    S_rplacd, S_starstar, S_samenamep, S_stringp, S_sub1,
    S_suspend, S_symbol_chars, S_symbolp, S_terpri, S_times,
    S_vector_copy, S_vectorp, S_vref, S_vset, S_vsize, S_write_byte;

cell KL_glob_syms;

static cell rdch(void) {
    cell c;

    if (Rejected != 255) {
        c = Rejected;
        Rejected = 255;
        return c;
    }
    if (Inptr >= Inlim) {
        Inlim = read(Infd, bytevec(Inbuf), BUFLEN);
        if (Inlim < 0) Inlim = 0;
        if (Inlim < 1) return ENDOFFILE;
        Inptr = 0;
    }
    return bytevec(Inbuf)[Inptr++];
}

int Parens;

cell KL_macro_env;

#ifdef BOOTSTRAP

static cell rdchci(void) {
    cell c;

    c = rdch();
    if (ENDOFFILE == c) return c;
    return tolower(c);
}

static cell length(cell n) {
    cell k = 0;

    while (!atomp(n)) {
        n = cdr(n);
        k++;
    }
    return k;
}

static cell findmac(cell x) {
    cell p;

    p = car(KL_macro_env);
    while (p != NIL) {
        if (caar(p) == x) return car(p);
        p = cdr(p);
    }
    return UNDEF;
}

static cell pop_infile(void);
static cell pop_outfile(void);

static void resetfiles(void) {
    while (Istack != NIL) pop_infile();
    while (Ostack != NIL) pop_outfile();
}

static cell xread(void);

static cell rdlist(void) {
    cell n, lst, a, count;
    char *badpair;

    badpair = "bad pair";
    Parens++;
    lst = cons(NIL, NIL);
    save(lst);
    a = NIL;
    count = 0;
    for (;;) {
        if (Error) return NIL;
        n = xread();
        if (EOTOBJ == n) error("missing ')'", UNDEF);
        if (S_dot == n) {
            if (count < 1) error(badpair, UNDEF);
            n = xread();
            rplacd(a, n);
            if (S_rparen == n || xread() != S_rparen)
                error(badpair, UNDEF);
            unsave(1);
            Parens--;
            return lst;
        }
        if (S_rparen == n) break;
        if (NIL == a)
            a = lst;
        else
            a = cdr(a);
        rplaca(a, n);
        rplacd(a, cons(NIL, NIL));
        count++;
    }
    Parens--;
    if (a != NIL) rplacd(a, NIL);
    unsave(1);
    return count ? lst : NIL;
}

static int symbolic(int c) {
    if (ENDOFFILE == c) return 0;
    return islower(c) || isdigit(c) ||
           strchr("*<=>+-", c);
}

static cell strsym(char *s);

static cell aton(char *s) {
    char *p;
    cell v, u;

    p = s;
    v = 0;
    while (*s) {
        if (v && FIXLIM / v < 10) error("overflow", strsym(p));
        v *= 10;
        u = *s - '0';
        if (v > FIXLIM - u) error("overflow", strsym(p));
        v += u;
        s++;
    }
    return v;
}

static int numeric(char *s) {
    cell v;

    v = 0;
    while (*s) {
        if (!isdigit(*s)) return 0;
        s++;
    }
    return 1;
}

static cell rdsym(int c) {
    char s[SYMLEN + 1];
    int i;

    i = 0;
    while (symbolic(c) || '/' == c) {
        if ('/' == c) c = rdch();
        if (SYMLEN == i)
            error("long symbol", UNDEF);
        else if (i < SYMLEN) {
            s[i] = c;
            i++;
        }
        c = rdchci();
    }
    s[i] = 0;
    Rejected = c;
    if (!strcmp(s, "nil")) return NIL;
    if (numeric(s)) return mkfix(aton(s));
    return addsym(s, UNDEF);
}

static cell rdstr(void) {
    char b[BUFLEN + 1];
    int k, i;
    cell c, s;
    byte *p;

    k = 0;
    c = rdch();
    while (c != '"') {
        if (ENDOFFILE == c) error("end of text", UNDEF);
        if ('\\' == c) c = rdch();
        if (BUFLEN == k)
            error("long string", UNDEF);
        else if (k < BUFLEN) {
            b[k] = c;
            k++;
        }
        c = rdch();
    }
    b[k] = 0;
    s = newstr(k, 128);
    p = string(s);
    for (i = 0; i < k; i++) p[i] = b[i];
    return s;
}

static void syntax(int x) { error("syntax", x); }

static cell rdbytevec(void) {
    cell n, p, v, k, i;
    byte *b;

    rdch();
    n = rdlist();
    k = 0;
    for (p = n; !atomp(p); p = cdr(p)) {
        if (!fixp(car(p)) || fixval(car(p)) > 255)
            error("bytevec range", car(p));
        k = k + 1;
    }
    save(n);
    if (p != NIL) syntax(n);
    v = newbytevec(k, 0);
    b = bytevec(v);
    p = n;
    for (i = 0; i < k; i++) {
        b[i] = fixval(car(p));
        p = cdr(p);
    }
    unsave(1);
    return v;
}

static cell rdvec(void) {
    cell n, p, m, *v, k, i;

    n = rdlist();
    k = 0;
    for (p = n; !atomp(p); p = cdr(p))
        k = k + 1;
    if (p != NIL) syntax(n);
    if (0 == k) return nullvec();
    save(n);
    m = newvec(TVEC, k, 0);
    v = vector(m);
    p = n;
    for (i = 0; i < k; i++) {
        v[i] = car(p);
        p = cdr(p);
    }
    unsave(1);
    return m;
}

static cell quote(cell q, cell n) { return cons(q, cons(n, NIL)); }

static cell xread(void) {
    cell c;

    c = rdchci();
    for (;;) {
        while (' ' == c || '\t' == c || '\n' == c || '\r' == c) {
            if (Error) return NIL;
            c = rdchci();
        }
        if (c != ';') break;
        while (c != '\n') c = rdchci();
    }
    if (ENDOFFILE == c || '%' == c) return EOTOBJ;
    if ('(' == c) {
        return rdlist();
    } else if ('\'' == c) {
        return quote(S_quote, xread());
    } else if ('@' == c || '`' == c) {
        return quote(S_quasiquote, xread());
    } else if (',' == c) {
        c = rdchci();
        if ('@' == c) return quote(S_unquote_splice, xread());
        Rejected = c;
        return quote(S_unquote, xread());
    } else if (')' == c) {
        if (!Parens) error("extra paren", UNDEF);
        return S_rparen;
    } else if ('.' == c) {
        if (!Parens) error("free dot", UNDEF);
        return S_dot;
    } else if ('/' == c || symbolic(c)) {
        return rdsym(c);
    } else if ('"' == c) {
        return rdstr();
    } else if ('#' == c) {
        c = rdchci();
        if ('(' == c)
            return rdvec();
        else if ('b' == c)
            return rdbytevec();
        else if ('\\' == c)
            return mkfix(rdch());
        else
            syntax(UNDEF);
        return UNDEF;
    } else {
        syntax(UNDEF);
        return UNDEF;
    }
}

static void slashify(char *s) {
    char b[SYMLEN * 2 + 1];
    int i, j;

    j = 0;
    for (i = 0; s[i]; i++) {
        if (!symbolic(s[i])) {
            b[j] = '/';
            j++;
        }
        b[j] = s[i];
        j++;
    }
    b[j] = 0;
    pr(b);
}

static void escape(cell n) {
    char b[2];
    int i, k;
    byte *s;

    s = string(n);
    k = stringlen(n);
    pr("\"");
    b[1] = 0;
    for (i = 0; i < k; i++) {
        if ('\\' == s[i] || '"' == s[i]) {
            b[0] = '\\';
            pr(b);
        }
        b[0] = s[i];
        pr(b);
    }
    pr("\"");
}

int Terse;

static void more(int k) {
    pr("#<+ ");
    pr(ntoa(k));
    pr(" more>");
}

#define PRDEPTH 128

static void prin2(cell n, int s, int d) {
    cell i, k;

    if (d > PRDEPTH) error("print depth", UNDEF);
    if (Error) error("stop", UNDEF);
    if (NIL == n) {
        pr("nil");
    } else if (EOTOBJ == n) {
        pr("#<eot>");
    } else if (symbolp(n)) {
        if (s)
            slashify(symstr(n));
        else
            pr(symstr(n));
    } else if (stringp(n)) {
        if (s)
            escape(n);
        else
            blockwrite((char *)string(n), stringlen(n));
    } else if (funp(n)) {
        pr("#<function ");
        prin2(caddr(n), s, d);
        pr(">");
    } else if (cfnp(n)) {
        pr("#<compiled-function>");
    } else if (esccontp(n)) {
        pr("#<escape-continuation>");
    } else if (fixp(n)) {
        pr(ntoa(fixval(n)));
    } else if (vectorp(n)) {
        pr("#(");
        k = veclen(n);
        for (i = 0; i < k; i++) {
            if (Terse && i >= 6) {
                more(k - i);
                break;
            }
            prin2(vector(n)[i], s, d + 1);
            if (i < k - 1) pr(" ");
        }
        pr(")");
    } else if (bytevecp(n)) {
        pr("#b(");
        k = nbytes(n);
        for (i = 0; i < k; i++) {
            if (Terse && i >= 5) {
                more(k - i);
                break;
            }
            pr(ntoa(bytevec(n)[i]));
            if (i < k - 1) pr(" ");
        }
        pr(")");
    } else if (atomp(n)) {
        pr("#<unprintable>");
    } else { /* List */
        pr("(");
        i = 0;
        while (n != NIL) {
            if (Terse && ++i >= 7) {
                more(length(n));
                break;
            }
            prin2(car(n), s, d + 1);
            n = cdr(n);
            if (atomp(n) && n != NIL) {
                pr(" . ");
                prin2(n, s, d + 1);
                n = NIL;
            } else if (n != NIL) {
                pr(" ");
            }
        }
        pr(")");
    }
}

static void prin1(cell n) {
    Terse = 0;
    prin2(n, 1, 0);
}

static void princ(cell n) {
    Terse = 0;
    prin2(n, 0, 0);
}

static void prinm(cell n) {
    Terse = 1;
    prin2(n, 1, 0);
}

#endif /* BOOTSTRAP */

static cell lookup(cell x) {
    cell a, e;

    for (e = Env; e != NIL; e = cdr(e)) {
        for (a = car(e); a != NIL; a = cdr(a)) {
            if (caar(a) == x)
                return cdar(a);
        }
    }
    return cdr(x);
}

static void vmerror3(char *s, char *m, cell x) {
    char b[SYMLEN];

    strcpy(b, s);
    strcat(b, ": ");
    strcat(b, m);
    error(b, x);
}

static void vmtype(char *s, cell x) {
    vmerror3(s, "type", x);
}

static void vmrange(char *s, cell x) {
    vmerror3(s, "range", x);
}

static void vmsize(char *s, cell x) {
    vmerror3(s, "size", x);
}

static void vmoverflow(char *s, cell x) {
    vmerror3(s, "overflow", x);
}

void vmnotimp(char *s) {
    vmerror3(s, "not implemented", UNDEF);
}

static void dowrite(int fd, void *b, int k) {
    if (write(fd, b, k) != k)
        vmerror3("suspend", "write error", UNDEF);
}

static cell suspend(cell x) {
    int fd, k, bo;
    byte buf[16];
    char w[] = "suspend";

    if (!symbolp(x) && !stringp(x)) vmtype(w, x);
    fd = creat(symbolp(x) ? symstr(x) : stringval(x), 0644);
    if (fd < 0) vmerror3(w, "file creation", x);
    memset(buf, '_', 16);
    memcpy(buf, MAGIC, strlen(MAGIC));
    k = strlen(MAGIC);
    buf[k] = WORDSIZE + '0';
    bo = 0x31323334;
    memcpy(&buf[k + 1], &bo, sizeof(int));
    putword(buf, k + 1 + sizeof(int), NCELLS);
    rplaca(TOPSYM, KL_glob_syms);
    dowrite(fd, buf, 16);
    dowrite(fd, Car, NCELLS * sizeof(cell));
    dowrite(fd, Cdr, NCELLS * sizeof(cell));
    dowrite(fd, Tag, NCELLS);
    dowrite(fd, Vectors, NVCELLS * sizeof(cell));
    close(fd);
    return S_t;
}

static void doread(int fd, void *b, int k) {
    if (read(fd, b, k) != k)
        error("image read error", UNDEF);
}

static void fasload(char *s) {
    int fd, k, n, bo;
    byte buf[16];
    char *badimg;

    badimg = "bad image";
    fd = open(s, O_RDONLY);
    if (fd < 0) return;
    k = strlen(MAGIC);
    doread(fd, buf, 16);
    if (buf[k] != WORDSIZE + '0') error(badimg, UNDEF);
    memcpy(&bo, &buf[k + 1], sizeof(int));
    if (bo != 0x31323334) error(badimg, UNDEF);
    n = word(buf, k + 1 + sizeof(int));
    if (n != NCELLS || memcmp(buf, MAGIC, k) != 0)
        error(badimg, UNDEF);
    doread(fd, Car, NCELLS * sizeof(cell));
    doread(fd, Cdr, NCELLS * sizeof(cell));
    doread(fd, Tag, NCELLS);
    doread(fd, Vectors, NVCELLS * sizeof(cell));
    if (read(fd, buf, 1) != 0)
        error(badimg, UNDEF);
    close(fd);
    KL_glob_syms = car(TOPSYM);
}

static cell add1(cell x) {
    cell v;
    char w[] = "add1";

    if (!fixp(x)) vmtype(w, x);
    v = fixval(x);
    if (FIXLIM == v) vmoverflow(w, x);
    return mkfix(v + 1);
}

static cell sub1(cell x) {
    cell v;
    char w[] = "sub1";

    if (!fixp(x)) vmtype(w, x);
    v = fixval(x);
    if (0 == v) vmoverflow(w, x);
    return mkfix(v - 1);
}

static cell plus(cell x) {
    cell u, v, p;
    char w[] = "plus";

    v = 0;
    for (p = x; p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        u = fixval(car(p));
        if (v > FIXLIM - u) vmoverflow(w, x);
        v += u;
    }
    return mkfix(v);
}

static cell times(cell x) {
    cell u, v, p;
    char w[] = "times";

    v = 1;
    for (p = x; p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        u = fixval(car(p));
        if (v && u && FIXLIM / v < u) vmoverflow(w, x);
        v *= u;
    }
    return mkfix(v);
}

static cell difference(cell x) {
    cell v, u;
    cell p;

    if (!fixp(car(x))) vmtype("difference", x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype("difference", x);
        u = fixval(car(p));
        if (v < u) vmoverflow("difference", x);
        v -= u;
    }
    return mkfix(v);
}

static cell divide(cell x, cell y) {
    char w[] = "divide";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, y);
    if (fixval(y) == 0) vmerror3(w, "divide by zero", x);
    return mkfix(fixval(x) / fixval(y));
}

static cell rem(cell x, cell y) {
    char w[] = "rem";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, y);
    if (fixval(y) == 0) vmerror3(w, "divide by zero", x);
    return mkfix(fixval(x) % fixval(y));
}

static cell eqv(cell x) {
    cell v, p;
    char w[] = "eqv";

    if (!fixp(car(x))) vmtype(w, x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        if (v != fixval(car(p))) return NIL;
        v = fixval(car(p));
    }
    return S_t;
}

static cell lessp(cell x) {
    cell v, p;
    char w[] = "lessp";

    if (!fixp(car(x))) vmtype(w, x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        if (v >= fixval(car(p))) return NIL;
        v = fixval(car(p));
    }
    return S_t;
}

static cell greaterp(cell x) {
    cell v, p;
    char w[] = "greaterp";

    if (!fixp(car(x))) vmtype(w, x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        if (v <= fixval(car(p))) return NIL;
        v = fixval(car(p));
    }
    return S_t;
}

static cell not_lessp(cell x) {
    cell v, p;
    char w[] = "not-lessp";

    if (!fixp(car(x))) vmtype(w, x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        if (v < fixval(car(p))) return NIL;
        v = fixval(car(p));
    }
    return S_t;
}

static cell not_greaterp(cell x) {
    cell v, p;
    char w[] = "not-greaterp";

    if (!fixp(car(x))) vmtype(w, x);
    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        if (v > fixval(car(p))) return NIL;
        v = fixval(car(p));
    }
    return S_t;
}

static cell logand(cell x) {
    cell v, p;

    v = FIXLIM;
    for (p = x; p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype("logand", x);
        v &= fixval(car(p));
    }
    return mkfix(v);
}

static cell lognot(cell x) {
    if (!fixp(x)) vmtype("lognot", x);
    return mkfix(~fixval(x) & WORDMASK);
}

static cell logor(cell x) {
    cell v, p;

    v = 0;
    for (p = x; p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype("logor", x);
        v |= fixval(car(p));
    }
    return mkfix(v);
}

static cell logxor(cell x) {
    cell v, p;

    v = fixval(car(x));
    for (p = cdr(x); p != NIL; p = cdr(p)) {
        if (!fixp(car(p))) vmtype("logxor", x);
        v ^= fixval(car(p));
    }
    return mkfix(v);
}

static cell leftshift(cell x, cell y) {
    cell v;
    char w[] = "leftshift";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, y);
    v = fixval(y);
    return mkfix((fixval(x) << v) & WORDMASK);
}

static cell rightshift(cell x, cell y) {
    cell v;
    char w[] = "rightshift";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, y);
    v = fixval(y);
    return mkfix(fixval(x) >> v);
}

static cell make_bytevec(cell x, cell y) {
    cell m;
    char w[] = "make-bytevec";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, x);
    m = fixval(y);
    if (m > 255) vmrange(w, y);
    return newbytevec(fixval(x), m);
}

static cell make_string(cell x, cell y) {
    cell m;
    char w[] = "make-string";

    if (!fixp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, x);
    m = fixval(y);
    if (m > 255) vmrange(w, y);
    return newstr(fixval(x), m);
}

cell make_vector(cell x, cell y) {
    cell k, n, *v, i;

    if (!fixp(x)) vmtype("make-vector", x);
    k = fixval(x);
    if (0 == k) return nullvec();
    n = newvec(TVEC, k, 0);
    v = vector(n);
    for (i = 0; i < k; i++) v[i] = y;
    return n;
}

static cell vref(cell x, cell y) {
    cell n;
    char w[] = "vref";

    if (!bytevecp(x) && !vectorp(x) && !stringp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, y);
    n = fixval(y);
    if (bytevecp(x)) {
        if (n >= nbytes(x)) vmsize(w, cons(x, cons(y, NIL)));
        return mkfix(bytevec(x)[n]);
    } else if (stringp(x)) {
        if (n >= stringlen(x)) vmsize(w, cons(x, cons(y, NIL)));
        return mkfix(string(x)[n]);
    } else {
        if (n >= veclen(x)) vmsize(w, cons(x, cons(y, NIL)));
        return vector(x)[n];
    }
}

static cell vset(cell x, cell y, cell z) {
    cell n, v;
    char w[] = "vset";

    if (!bytevecp(x) && !vectorp(x) && !stringp(x)) vmtype(w, x);
    if (!fixp(y)) vmtype(w, x);
    n = fixval(y);
    if (bytevecp(x) || stringp(x)) {
        if (!fixp(z)) vmtype(w, z);
        v = fixval(z);
        if (n >= nbytes(x)) vmsize(w, cons(x, cons(y, NIL)));
        if (v > 255) vmrange(w, z);
        if (stringp(x) && v > 127) vmrange(w, z);
        bytevec(x)[n] = v;
    } else {
        if (n >= veclen(x))
            vmsize(w, cons(x, cons(y, NIL)));
        vector(x)[n] = z;
    }
    return x;
}

static cell vector_copy(cell v, cell n0, cell n1, cell u, cell m0) {
    cell kv, kw, b, nn0, nn1, nm0;
    char w[] = "vector-copy";

    if (!fixp(n0)) vmtype(w, n0);
    if (!fixp(n1)) vmtype(w, n1);
    if (!fixp(m0)) vmtype(w, m0);
    b = 0;
    if (vectorp(v) && vectorp(u))
        b = 0;
    else if (stringp(v) && stringp(u))
        b = 1;
    else if (bytevecp(v) && bytevecp(u))
        b = 1;
    else
        vmtype(w, cons(v, cons(u, NIL)));
    nn0 = fixval(n0);
    nn1 = fixval(n1);
    nm0 = fixval(m0);
    kv = b ? (stringp(v) ? stringlen(v) : nbytes(v)) : veclen(v);
    kw = b ? (stringp(u) ? stringlen(u) : nbytes(u)) : veclen(u);
    if (nn0 > nn1 || nn1 > kv)
        vmsize(w, cons(v, cons(n0, cons(n1, NIL))));
    if (nn1 - nn0 + nm0 > kw)
        vmsize(w, cons(u, cons(m0, NIL)));
    if (b)
        memcpy(&bytevec(u)[nm0], &bytevec(v)[nn0], nn1 - nn0);
    else
        memcpy(&vector(u)[nm0], &vector(v)[nn0],
               (nn1 - nn0) * sizeof(cell));
    return u;
}

static cell read_byte(int peek) {
    cell c;

    c = rdch();
    if (peek && c != ENDOFFILE) Rejected = c;
    return ENDOFFILE == c ? EOTOBJ : mkfix(c);
}

static cell write_byte(cell x) {
    cell c;
    char b[1];
    char w[] = "write-byte";

    if (!fixp(x)) vmtype(w, x);
    c = fixval(x);
    if (c > 255) vmrange(w, x);
    b[0] = c;
    blockwrite(b, 1);
    return x;
}

static cell symbol_chars(cell x) {
    cell n;
    char *s;

    if (!symbolp(x)) vmtype("symbol-chars", x);
    save(n = cons(NIL, NIL));
    for (s = symstr(x); *s; s++) {
        rplaca(n, mkfix(*s));
        if (s[1]) {
            rplacd(n, cons(NIL, NIL));
            n = cdr(n);
        }
    }
    return unsave(1);
}

static cell maksym(cell x) {
    cell i, n, p;
    char w[] = "maksym";
    char b[SYMLEN + 1];

    if (atomp(x)) vmtype(w, x);
    i = 0;
    for (p = x; !atomp(p); p = cdr(p)) {
        if (!fixp(car(p))) vmtype(w, x);
        n = fixval(car(p));
        if (n < 32 || n > 126) vmrange(w, x);
        if (i >= SYMLEN) vmsize(w, x);
        b[i++] = fixval(car(p));
    }
    if (p != NIL) vmtype(w, x);
    b[i] = 0;
    return strsym(b);
}

static cell samenamep(cell x, cell y) {
    char w[] = "samenamep";

    if (!symbolp(x)) vmtype(w, x);
    if (!symbolp(y)) vmtype(w, y);
    x = car(x);
    y = car(y);
    while (x != NIL && y != NIL) {
        if (car(x) != car(y)) return NIL;
        x = cdr(x);
        y = cdr(y);
    }
    return NIL == x && NIL == y ? S_t : NIL;
}

static void push_infile(void) {
    Tmp = cons(mkfix(Inlim), NIL);
    Tmp = cons(mkfix(Inptr), Tmp);
    Tmp = cons(mkfix(Infd), Tmp);
    Tmp = cons(mkfix(Rejected), Tmp);
    Tmp = cons(Inbuf, Tmp);
    Istack = cons(Tmp, Istack);
    Tmp = NIL;
}

static void push_outfile(void) {
    Tmp = cons(mkfix(Outlim), NIL);
    Tmp = cons(mkfix(Outptr), Tmp);
    Tmp = cons(mkfix(Outfd), Tmp);
    Tmp = cons(Outbuf, Tmp);
    Ostack = cons(Tmp, Ostack);
    Tmp = NIL;
}

static cell pop_infile(void) {
    cell n;

    if (NIL == Istack) return NIL;
    if (Infd != 0) close(Infd);
    n = car(Istack);
    Istack = cdr(Istack);
    Inbuf = car(n);
    n = cdr(n);
    Rejected = fixval(car(n));
    n = cdr(n);
    Infd = fixval(car(n));
    n = cdr(n);
    Inptr = fixval(car(n));
    n = cdr(n);
    Inlim = fixval(car(n));
    return S_t;
}

static cell pop_outfile(void) {
    cell n;

    if (NIL == Ostack) return NIL;
    flush();
    if (Outfd != 1) close(Outfd);
    n = car(Ostack);
    Ostack = cdr(Ostack);
    Outbuf = car(n);
    n = cdr(n);
    Outfd = fixval(car(n));
    n = cdr(n);
    Outptr = fixval(car(n));
    n = cdr(n);
    Outlim = fixval(car(n));
    return S_t;
}

static int redir_input(char *s) {
    int fd;

    fd = NULL == s || 0 == *s ? 0 : open(s, O_RDONLY);
    if (fd < 0) return -1;
    Inbuf = newbytevec(BUFLEN, 0);
    Infd = fd;
    Inptr = 0;
    Inlim = 0;
    Rejected = 255;
    return 0;
}

static int redir_output(char *s, int app) {
    int fd;

    if (app) fd = open(s, O_WRONLY);
    if (!app || fd < 0) fd = NULL == s || 0 == *s ? 1 : creat(s, 0644);
    if (fd < 0) return -1;
    // if (app) lseek(fd, 0, SEEK_END);
    Outbuf = newbytevec(BUFLEN, 0);
    Outfd = fd;
    Outptr = 0;
    Outlim = 0;
    return 0;
}

static cell input_file(cell x) {
    char w[] = "input-file";
    char *s;

    if (NIL == x) return pop_infile();
    if (!symbolp(x) && !stringp(x)) vmtype(w, x);
    push_infile();
    s = stringp(x) ? stringval(x) : symstr(x);
    if (redir_input(s) < 0) vmerror3(w, "file", x);
    return S_t;
}

static cell output_file(cell x, int app) {
    char w[] = "output-file";
    char *s;

    if (NIL == x) return pop_outfile();
    if (!symbolp(x) && !stringp(x)) vmtype(w, x);
    push_outfile();
    s = stringp(x) ? stringval(x) : symstr(x);
    if (redir_output(s, app) < 0) vmerror3(w, "file", x);
    return S_t;
}

static cell delete_file(cell x) {
    char w[] = "delete-file";
    char *p;

    if (!symbolp(x) && !stringp(x)) vmtype(w, x);
    p = symbolp(x) ? symstr(x) : stringval(x);
    if (strlen(p) >= BUFLEN - 1) error("string too long", x);
    if (remove(p) < 0)
        vmerror3(w, "failed", x);
    return S_t;
}

static cell rename_file(cell x, cell y) {
    char w[] = "rename-file";
    char buf[BUFLEN + 1], *p;

    if (!symbolp(x) && !stringp(x)) vmtype(w, x);
    if (!symbolp(y) && !stringp(y)) vmtype(w, x);
    p = symbolp(x) ? symstr(x) : stringval(x);
    if (strlen(p) >= BUFLEN - 1) error("string too long", x);
    strcpy(buf, p);
    p = symbolp(y) ? symstr(y) : stringval(y);
    if (strlen(p) >= BUFLEN - 1) error("string too long", y);
    if (rename(buf, p) < 0) vmerror3(w, "failed", x);
    return S_t;
}

static cell user_gc(cell x) {
    cell n, m;

    Verbose_GC = x != NIL;
    Debug_GC = S_gc == x;
    if (Debug_GC) Verbose_GC = 0;
    n = gcv(&m);
    Tmp = mkfix(m);
    n = cons(Tmp, cons(mkfix(n), NIL));
    Tmp = NIL;
    return n;
}

#ifdef KLCLOCK

#include <sys/time.h>
#include <time.h>

struct timeval tv0, tv1;
int clock_running = 0;

static cell kl_clock(cell x) {
    long ds, du;
    cell n;

    if (x != NIL) {
        gettimeofday(&tv0, NULL);
        clock_running = 1;
        return NIL;
    } else {
        if (0 == clock_running) return NIL;
        gettimeofday(&tv1, NULL);
        ds = tv1.tv_sec - tv0.tv_sec;
        du = tv1.tv_usec - tv0.tv_usec;
        if (du < 0) {
            ds--;
            du = tv0.tv_usec - tv1.tv_usec;
        }
    }
    if (ds > FIXLIM) vmoverflow("kl-clock", UNDEF);
    du /= 10000;
    Tmp = cons(mkfix(du), NIL);
    n = cons(mkfix(ds), Tmp);
    Tmp = NIL;
    return n;
}

#else /* KLCLOCK */

cell kl_clock(cell x) {
    vmnotimp("kl-clock");
    return UNDEF;
}

#endif /* KLCLOCK */

#ifdef KLREADKEY

#include <termios.h>

static struct termios cooked, raw;

int TTYfd = -1;

cell keyboard_input(cell x) {
    if (TTYfd < 0) {
        TTYfd = open("/dev/tty", 0);
        if (TTYfd < 0) error("cannot open TTY", UNDEF);
        tcgetattr(TTYfd, &cooked);
        tcgetattr(TTYfd, &raw);
        cfmakeraw(&raw);
    }
    if (x != NIL)
        tcsetattr(TTYfd, TCSANOW | TCSADRAIN, &raw);
    else
        tcsetattr(TTYfd, TCSANOW | TCSADRAIN, &cooked);
    return S_t;
}

cell read_key(void) {
    char c;

    read(TTYfd, &c, 1);
    return mkfix(c);
}

#else /* KLREADKEY */

cell keyboard_input(cell x) {
    vmnotimp("keyboard-input");
    return UNDEF;
}

cell read_key(void) {
    vmnotimp("read-key");
    return UNDEF;
}

#endif /* KLREADKEY */

enum { OP_NOP,
       OP_APPLIS,
       OP_APPLIS_T,
       OP_APPLY,
       OP_APPLY_T,
       OP_ARG,
       OP_BRF,
       OP_BRT,
       OP_CATCH,
       OP_ENTCOL,
       OP_ENTER,
       OP_GLOBENV,
       OP_HALT,
       OP_JMP,
       OP_MKENV,
       OP_MKFRAME,
       OP_MKFUN,
       OP_PROPENV,
       OP_PUSH,
       OP_QUOTE,
       OP_REF,
       OP_RETURN,
       OP_SETARG,
       OP_SETGLOB,
       OP_SETREF,
       OP_THROW,
       OP_TOFRAME,

       OP_ADD1,
       OP_APPEND_FILE,
       OP_ATOM,
       OP_BYTEVECP,
       OP_CAR,
       OP_CDR,
       OP_CLOCK,
       OP_CONS,
       OP_DELETE_FILE,
       OP_DIFFERENCE,
       OP_DIVIDE,
       OP_ESCCONTP,
       OP_EOTP,
       OP_EQ,
       OP_EQV,
       OP_FINISH_OUTPUT,
       OP_FIXP,
       OP_FUNCTIONP,
       OP_GC,
       OP_GREATERP,
       OP_INPUT_FILE,
       OP_KL_CAR,
       OP_KL_CDR,
       OP_KL_CONS3,
       OP_KL_CRVAL,
       OP_KL_INDCR,
       OP_KL_KEYBOARD_INPUT,
       OP_KL_READ_KEY,
       OP_KL_RPLACA,
       OP_KL_RPLACD,
       OP_KL_TRACE,
       OP_LEFTSHIFT,
       OP_LESSP,
       OP_LOGAND,
       OP_LOGNOT,
       OP_LOGOR,
       OP_LOGXOR,
       OP_MAKE_BYTEVEC,
       OP_MAKE_STRING,
       OP_MAKE_VECTOR,
       OP_MAKSYM,
       OP_NOT_GREATERP,
       OP_NOT_LESSP,
       OP_OUTPUT_FILE,
       OP_PEEK_BYTE,
       OP_PLUS,
       OP_READ_BYTE,
       OP_REM,
       OP_RENAME_FILE,
       OP_RIGHTSHIFT,
       OP_RPLACA,
       OP_RPLACD,
       OP_SAMENAMEP,
       OP_STRINGP,
       OP_SUB1,
       OP_SUSPEND,
       OP_SYMBOL_CHARS,
       OP_SYMBOLP,
       OP_TIMES,
       OP_VECTOR_COPY,
       OP_VECTORP,
       OP_VREF,
       OP_VSET,
       OP_VSIZE,
       OP_WRITE_BYTE };

static cell apply(int tc) {
    cell n;

    if (!cfnp(Acc)) error("application of non-function", Acc);
    if (tc) {
        n = car(Stack);
        rplaca(Stack, cadr(Stack));
        rplaca(cdr(Stack), n);
    } else {
        n = cons3(VMip + 1,
                  cons(VMprog, cons(VMenv, VMargs)),
                  ATOM | TFIX);
        save(n);
    }
    VMprog = cadr(Acc);
    VMenv = caddr(Acc);
    return fixval(Acc);
}

static cell applis(int tc) {
    cell p;

    if (atomp(car(Stack)) && car(Stack) != NIL)
        vmtype("apply*", car(Stack));
    for (p = car(Stack); !atomp(p); p = cdr(p))
        ;
    if (p != NIL) vmtype("apply*", car(Stack));
    return apply(tc);
}

static void enter(cell na, int coll) {
    cell a, aa, i, x, k;

    aa = a = cadr(Stack);
    k = coll ? na + 1 : na;
    if (k != 0) VMargs = newvec(TVEC, k, 1);
    for (i = 0; i < na; i++) {
        if (NIL == a) error("too few arguments", cons(Acc, aa));
        x = cons(car(a), NIL);
        vector(VMargs)[i] = x;
        a = cdr(a);
    }
    if (a != NIL && 0 == coll) error("extra arguments", cons(Acc, aa));
    if (i < k) {
        x = cons(a, NIL);
        vector(VMargs)[i] = x;
    }
    rplacd(Stack, cddr(Stack));
}

static cell retn(void) {
    cell n;

    n = unsave(1);
    VMip = fixval(n);
    n = cdr(n);
    VMprog = car(n);
    n = cdr(n);
    VMenv = car(n);
    VMargs = cdr(n);
    return VMip;
}

static cell esccont(void) {
    cell n;

    n = cons(VMargs, Stack);
    n = cons(VMenv, n);
    n = cons(VMprog, n);
    return cons3(VMip + 2, n, ATOM | TESC);
}

static cell throwtag(cell tag) {
    cell n;

    if (!esccontp(tag)) vmtype("throw*", tag);
    n = unsave(1);
    restcont(tag);
    Acc = n;
    return VMip;
}

#define skip(n) (VMip += (n))

#ifdef KL24BIT

#define skip0() skip(1)
#define skip1() skip(4)
#define skip2() skip(7)
#define skipn(n) skip(4 + (n)*3)

#else

#define skip0() skip(1)
#define skip1() skip(3)
#define skip2() skip(5)
#define skipn(n) skip(3 + (n)*2)

#endif /* KL24BIT */

static cell vmarg1(void) {
    byte *b;

    b = bytevec(VMprog);
    return word(b, VMip + 1);
}

static cell vmarg2(void) {
    byte *b;

    b = bytevec(VMprog);
    return word(b, VMip + 1 + WORDSIZE);
}

static void growenv(cell k) {
    cell n, x, old;
    int i;

    k += 128;
    n = newvec(TVEC, k, 1);
    save(n);
    old = veclen(car(VMGLOB));
    if (old != 0) memcpy(vector(n), vector(car(VMGLOB)),
                         old * sizeof(cell));
    for (i = old; i < k; i++) {
        x = cons(UNDEF, NIL);
        vector(n)[i] = x;
    }
    n = unsave(1);
    if (VMenv == car(VMGLOB)) VMenv = n;
    rplaca(VMGLOB, n);
}

static void cpenv(cell n, cell tbl) {
    cell i, j, a;
    byte *b;

    n *= WORDSIZE;
    b = bytevec(VMprog);
    for (i = j = 0; i < n; i += WORDSIZE) {
        a = word(b, tbl + i);
#ifdef KL24BIT
        if (a & 0x800000)
            vector(Acc)[j] = vector(VMargs)[a & 0x7fffff];
#else
        if (a & 0x8000)
            vector(Acc)[j] = vector(VMargs)[a & 0x7fff];
#endif /* KL24BIT */
        else {
            /* In case of nested DEFINE */
            if (a >= veclen(VMenv)) growenv(veclen(VMenv));
            vector(Acc)[j] = vector(VMenv)[a];
        }
        j++;
    }
}

cell Trace[10];
cell Tracept;

cell loadtrace(void) {
    cell n, i, j;

    n = NIL;
    j = Tracept;
    for (i = 0; i < 10; i++) {
        n = cons(Trace[j], n);
        j++;
        if (j >= 10) j = 0;
    }
    return nrev(n);
}

static cell run(cell p) {
    cell n, op;

    VMprog = cadr(p);
    VMip = fixval(p);
    VMenv = caddr(p);
    save(NIL);
    save(NIL);
    VMrun = 1;
    for (;;) {
        op = bytevec(VMprog)[VMip];
        if (Error) error("interrupt", UNDEF);
        switch (op) {
            case OP_APPLIS:
                VMip = applis(0);
                break;
            case OP_APPLIS_T:
                VMip = applis(1);
                break;
            case OP_APPLY:
                VMip = apply(0);
                break;
            case OP_APPLY_T:
                VMip = apply(1);
                break;
            case OP_ARG:
                Acc = car(vector(VMargs)[vmarg1()]);
                skip1();
                break;
            case OP_BRF:
                if (NIL == Acc)
                    VMip = vmarg1();
                else
                    skip1();
                break;
            case OP_BRT:
                if (Acc != NIL)
                    VMip = vmarg1();
                else
                    skip1();
                break;
            case OP_CATCH:
                save(cons(esccont(), NIL));
                skip0();
                break;
            case OP_ENTCOL:
                enter(vmarg1(), 1);
                skip1();
                break;
            case OP_ENTER:
                enter(vmarg1(), 0);
                skip1();
                break;
            case OP_GLOBENV:
                Acc = car(VMGLOB);
                skip0();
                break;
            case OP_HALT:
                flush();
                exit(0);
                break;
            case OP_JMP:
                VMip = vmarg1();
                break;
            case OP_MKENV:
                n = vmarg1();
                Acc = newvec(TVEC, n, 1);
                cpenv(n, VMip + 1 + WORDSIZE);
                skipn(n);
                break;
            case OP_MKFUN:
                n = cons3(vmarg1(), cons(VMprog, cons(Acc, NIL)),
                          ATOM | TCFN);
                Acc = n;
                skip1();
                break;
            case OP_MKFRAME:
                save(NIL);
                skip0();
                break;
            case OP_NOP:
                skip0();
                break;
            case OP_PROPENV:
                Acc = VMenv;
                skip0();
                break;
            case OP_PUSH:
                save(Acc);
                skip0();
                break;
            case OP_QUOTE:
                Acc = vmarg1();
                skip1();
                break;
            case OP_REF:
                Acc = car(vector(VMenv)[vmarg1()]);
                n = vmarg2();
                if (UNDEF == Acc) error("undefined", n);
                Trace[Tracept] = n;
                Tracept++;
                if (Tracept >= 10) Tracept = 0;
                skip2();
                break;
            case OP_RETURN:
#ifdef BOOTSTRAP
                if (NIL == car(Stack)) {
                    unsave(1);
                    VMrun = 0;
                    return Acc;
                }
#endif /* BOOTSTRAP */
                VMip = retn();
                break;
            case OP_SETARG:
                rplaca(vector(VMargs)[vmarg1()], Acc);
                skip1();
                break;
            case OP_SETGLOB:
                n = vmarg1();
                if (n >= veclen(car(VMGLOB))) growenv(n);
                rplaca(vector(car(VMGLOB))[n], Acc);
                skip1();
                break;
            case OP_SETREF:
                rplaca(vector(VMenv)[vmarg1()], Acc);
                skip1();
                break;
            case OP_THROW:
                VMip = throwtag(Acc);
                break;
            case OP_TOFRAME:
                rplaca(Stack, cons(Acc, car(Stack)));
                skip0();
                break;
            /*--- FUNCTIONS ---*/
            case OP_ADD1:
                Acc = add1(Acc);
                skip0();
                break;
            case OP_APPEND_FILE:
                Acc = output_file(Acc, 1);
                skip0();
                break;
            case OP_ATOM:
                Acc = atomp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_BYTEVECP:
                Acc = bytevecp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_CAR:
                if (atomp(Acc) && Acc != NIL) vmtype("car", Acc);
                Acc = car(Acc);
                skip0();
                break;
            case OP_CDR:
                if (atomp(Acc) && Acc != NIL) vmtype("cdr", Acc);
                Acc = cdr(Acc);
                skip0();
                break;
            case OP_CLOCK:
                Acc = kl_clock(Acc);
                skip0();
                break;
            case OP_CONS:
                Acc = cons(Acc, unsave(1));
                skip0();
                break;
            case OP_DELETE_FILE:
                Acc = delete_file(Acc);
                skip0();
                break;
            case OP_DIFFERENCE:
                Acc = difference(unsave(1));
                skip0();
                break;
            case OP_DIVIDE:
                Acc = divide(Acc, unsave(1));
                skip0();
                break;
            case OP_ESCCONTP:
                Acc = esccontp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_EOTP:
                Acc = EOTOBJ == Acc ? S_t : NIL;
                skip0();
                break;
            case OP_EQ:
                Acc = Acc == unsave(1) ? S_t : NIL;
                skip0();
                break;
            case OP_EQV:
                Acc = eqv(unsave(1));
                skip0();
                break;
            case OP_FINISH_OUTPUT:
                flush();
                Acc = NIL;
                skip0();
                break;
            case OP_FIXP:
                Acc = fixp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_FUNCTIONP:
                Acc = cfnp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_GC:
                Acc = user_gc(Acc);
                skip0();
                break;
            case OP_GREATERP:
                Acc = greaterp(unsave(1));
                skip0();
                break;
            case OP_INPUT_FILE:
                Acc = input_file(Acc);
                skip0();
                break;
            case OP_KL_CAR:
                Acc = car(Acc);
                skip0();
                break;
            case OP_KL_CDR:
                Acc = cdr(Acc);
                skip0();
                break;
            case OP_KL_CONS3:
                n = unsave(1);
                Acc = cons3(Acc, n, fixval(unsave(1)));
                skip0();
                break;
            case OP_KL_CRVAL:
                Acc = fixval(Acc);
                if (Acc >= NCELLS) error("bad address", UNDEF);
                skip0();
                break;
            case OP_KL_INDCR:
                Acc = mkfix(Acc);
                skip0();
                break;
            case OP_KL_KEYBOARD_INPUT:
                Acc = keyboard_input(Acc);
                skip0();
                break;
            case OP_KL_READ_KEY:
                Acc = read_key();
                skip0();
                break;
            case OP_KL_RPLACA:
                rplaca(Acc, unsave(1));
                skip0();
                break;
            case OP_KL_RPLACD:
                rplacd(Acc, unsave(1));
                skip0();
                break;
            case OP_KL_TRACE:
                Acc = loadtrace();
                skip0();
                break;
            case OP_LESSP:
                Acc = lessp(unsave(1));
                skip0();
                break;
            case OP_LEFTSHIFT:
                Acc = leftshift(Acc, unsave(1));
                skip0();
                break;
            case OP_LOGAND:
                Acc = logand(unsave(1));
                skip0();
                break;
            case OP_LOGNOT:
                Acc = lognot(Acc);
                skip0();
                break;
            case OP_LOGOR:
                Acc = logor(unsave(1));
                skip0();
                break;
            case OP_LOGXOR:
                Acc = logxor(unsave(1));
                skip0();
                break;
            case OP_MAKE_BYTEVEC:
                Acc = make_bytevec(Acc, unsave(1));
                skip0();
                break;
            case OP_MAKE_STRING:
                Acc = make_string(Acc, unsave(1));
                skip0();
                break;
            case OP_MAKE_VECTOR:
                Acc = make_vector(Acc, unsave(1));
                skip0();
                break;
            case OP_MAKSYM:
                Acc = maksym(Acc);
                skip0();
                break;
            case OP_NOT_GREATERP:
                Acc = not_greaterp(unsave(1));
                skip0();
                break;
            case OP_NOT_LESSP:
                Acc = not_lessp(unsave(1));
                skip0();
                break;
            case OP_OUTPUT_FILE:
                Acc = output_file(Acc, 0);
                skip0();
                break;
            case OP_PEEK_BYTE:
                Acc = read_byte(1);
                skip0();
                break;
            case OP_PLUS:
                Acc = plus(unsave(1));
                skip0();
                break;
            case OP_READ_BYTE:
                Acc = read_byte(0);
                skip0();
                break;
            case OP_REM:
                Acc = rem(Acc, unsave(1));
                skip0();
                break;
            case OP_RENAME_FILE:
                Acc = rename_file(Acc, unsave(1));
                skip0();
                break;
            case OP_RIGHTSHIFT:
                Acc = rightshift(Acc, unsave(1));
                skip0();
                break;
            case OP_RPLACA:
                if (atomp(Acc)) vmtype("rplaca", Acc);
                rplaca(Acc, unsave(1));
                skip0();
                break;
            case OP_RPLACD:
                if (atomp(Acc)) vmtype("rplacd", Acc);
                rplacd(Acc, unsave(1));
                skip0();
                break;
            case OP_SAMENAMEP:
                Acc = samenamep(Acc, unsave(1));
                skip0();
                break;
            case OP_STRINGP:
                Acc = stringp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_SUB1:
                Acc = sub1(Acc);
                skip0();
                break;
            case OP_SUSPEND:
                Acc = suspend(Acc);
                skip0();
                break;
            case OP_SYMBOL_CHARS:
                Acc = symbol_chars(Acc);
                skip0();
                break;
            case OP_SYMBOLP:
                Acc = symbolp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_TIMES:
                Acc = times(unsave(1));
                skip0();
                break;
            case OP_VECTOR_COPY:
                n = unsave(1);
                Acc = vector_copy(car(n), cadr(n), caddr(n),
                                  cadddr(n), car(cddddr(n)));
                skip0();
                break;
            case OP_VECTORP:
                Acc = vectorp(Acc) ? S_t : NIL;
                skip0();
                break;
            case OP_VREF:
                Acc = vref(Acc, unsave(1));
                skip0();
                break;
            case OP_VSET:
                n = unsave(1);
                Acc = vset(Acc, n, unsave(1));
                skip0();
                break;
            case OP_VSIZE:
                if (vectorp(Acc))
                    Acc = mkfix(veclen(Acc));
                else if (stringp(Acc))
                    Acc = mkfix(stringlen(Acc));
                else if (bytevecp(Acc))
                    Acc = mkfix(nbytes(Acc));
                else
                    vmtype("vsize", Acc);
                skip0();
                break;
            case OP_WRITE_BYTE:
                Acc = write_byte(Acc);
                skip0();
                break;
            default:
                error("illegal instruction", mkfix(op));
                VMrun = 0;
                return UNDEF;
        }
    }
    return UNDEF;
}

#ifdef BOOTSTRAP

static cell eval(cell x);

static void load(char *s) {
    push_infile();
    if (redir_input(s) < 0) error("load", strsym(s));
    for (;;) {
        Acc = xread();
        if (EOTOBJ == Acc) break;
        eval(Acc);
    }
    pop_infile();
}

static void check(cell x, int k0, int kn) {
    int i, a;

    i = 0;
    for (a = x; !atomp(a); a = cdr(a))
        i++;
    if (a != NIL || i < k0 || (kn != -1 && i > kn))
        syntax(x);
}

static void wrongtype(cell x) { error("type", x); }

static cell builtin(cell x) {
    cell n;
    char *s;

    if (S_car == car(x)) {
        check(x, 2, 2);
        n = cadr(x);
        if (atomp(n) && n != NIL) wrongtype(x);
        return caadr(x);
    } else if (S_cdr == car(x)) {
        check(x, 2, 2);
        n = cadr(x);
        if (atomp(n) && n != NIL) wrongtype(x);
        return cdadr(x);
    } else if (S_eq == car(x)) {
        check(x, 3, 3);
        return cadr(x) == caddr(x) ? S_t : NIL;
    } else if (S_atom == car(x)) {
        check(x, 2, 2);
        return atomp(cadr(x)) ? S_t : NIL;
    } else if (S_cons == car(x)) {
        check(x, 3, 3);
        return cons(cadr(x), caddr(x));
    } else if (S_rplaca == car(x)) {
        check(x, 3, 3);
        if (atomp(cadr(x))) wrongtype(x);
        rplaca(cadr(x), caddr(x));
        return cadr(x);
    } else if (S_rplacd == car(x)) {
        check(x, 3, 3);
        if (atomp(cadr(x))) wrongtype(x);
        rplacd(cadr(x), caddr(x));
        return cadr(x);
    } else if (S_symbolp == car(x)) {
        check(x, 2, 2);
        return symbolp(cadr(x)) ? S_t : NIL;
    } else if (S_fixnump == car(x)) {
        check(x, 2, 2);
        return fixp(cadr(x)) ? S_t : NIL;
    } else if (S_functionp == car(x)) {
        check(x, 2, 2);
        return funp(cadr(x)) || cfnp(cadr(x)) ? S_t : NIL;
    } else if (S_vectorp == car(x)) {
        check(x, 2, 2);
        return vectorp(cadr(x)) ? S_t : NIL;
    } else if (S_stringp == car(x)) {
        check(x, 2, 2);
        return stringp(cadr(x)) ? S_t : NIL;
    } else if (S_bytevecp == car(x)) {
        check(x, 2, 2);
        return bytevecp(cadr(x)) ? S_t : NIL;
    } else if (S_eqv == car(x)) {
        check(x, 2, -1);
        return eqv(cdr(x));
    } else if (S_add1 == car(x)) {
        check(x, 2, 2);
        return add1(cadr(x));
    } else if (S_plus == car(x)) {
        check(x, 0, -1);
        return plus(cdr(x));
    } else if (S_sub1 == car(x)) {
        check(x, 2, 2);
        return sub1(cadr(x));
    } else if (S_difference == car(x)) {
        check(x, 2, -1);
        return difference(cdr(x));
    } else if (S_lessp == car(x)) {
        check(x, 2, -1);
        return lessp(cdr(x));
    } else if (S_not_lessp == car(x)) {
        check(x, 2, -1);
        return not_lessp(cdr(x));
    } else if (S_greaterp == car(x)) {
        check(x, 2, -1);
        return greaterp(cdr(x));
    } else if (S_not_greaterp == car(x)) {
        check(x, 2, -1);
        return not_greaterp(cdr(x));
    } else if (S_logand == car(x)) {
        check(x, 0, -1);
        return logand(cdr(x));
    } else if (S_logor == car(x)) {
        check(x, 0, -1);
        return logor(cdr(x));
    } else if (S_logxor == car(x)) {
        check(x, 1, -1);
        return logxor(cdr(x));
    } else if (S_leftshift == car(x)) {
        check(x, 3, 3);
        return leftshift(cadr(x), caddr(x));
    } else if (S_rightshift == car(x)) {
        check(x, 3, 3);
        return rightshift(cadr(x), caddr(x));
    } else if (S_lognot == car(x)) {
        check(x, 2, 2);
        return lognot(cadr(x));
    } else if (S_times == car(x)) {
        check(x, 0, -1);
        return times(cdr(x));
    } else if (S_divide == car(x)) {
        check(x, 3, 3);
        return divide(cadr(x), caddr(x));
    } else if (S_rem == car(x)) {
        check(x, 3, 3);
        return rem(cadr(x), caddr(x));
    } else if (S_symbol_chars == car(x)) {
        check(x, 2, 2);
        return symbol_chars(cadr(x));
    } else if (S_maksym == car(x)) {
        check(x, 2, 2);
        return maksym(cadr(x));
    } else if (S_samenamep == car(x)) {
        check(x, 3, 3);
        return samenamep(cadr(x), caddr(x));
    } else if (S_kl_cons3 == car(x)) {
        check(x, 4, 4);
        return cons3(cadr(x), caddr(x), fixval(cadddr(x)));
    } else if (S_kl_car == car(x)) {
        check(x, 2, 2);
        return caadr(x);
    } else if (S_kl_cdr == car(x)) {
        check(x, 2, 2);
        return cdadr(x);
    } else if (S_kl_rplacd == car(x)) {
        check(x, 3, 3);
        /* Must be NOP, used in KL-SET-FNAME */
        return cadr(x);
    } else if (S_kl_crval == car(x)) {
        check(x, 2, 2);
        if (!fixp(cadr(x))) wrongtype(x);
        return fixval(cadr(x));
    } else if (S_vector_copy == car(x)) {
        check(x, 6, 6);
        return vector_copy(cadr(x), caddr(x), cadddr(x),
                           car(cddddr(x)), cadr(cddddr(x)));
    } else if (S_make_bytevec == car(x)) {
        check(x, 2, 3);
        return NIL == cddr(x) ? make_bytevec(cadr(x), mkfix(0)) : make_bytevec(cadr(x), caddr(x));
    } else if (S_make_string == car(x)) {
        check(x, 2, 3);
        return NIL == cddr(x) ? make_string(cadr(x), mkfix(' ')) : make_string(cadr(x), caddr(x));
    } else if (S_make_vector == car(x)) {
        check(x, 2, 3);
        return NIL == cddr(x) ? make_vector(cadr(x), NIL) : make_vector(cadr(x), caddr(x));
    } else if (S_vref == car(x)) {
        check(x, 3, 3);
        return vref(cadr(x), caddr(x));
    } else if (S_vset == car(x)) {
        check(x, 4, 4);
        return vset(cadr(x), caddr(x), cadddr(x));
    } else if (S_vsize == car(x)) {
        check(x, 2, 2);
        if (vectorp(cadr(x)))
            return mkfix(veclen(cadr(x)));
        if (stringp(cadr(x)))
            return mkfix(stringlen(cadr(x)));
        if (bytevecp(cadr(x)))
            return mkfix(nbytes(cadr(x)));
        wrongtype(x);
        return NIL;
    } else if (S_kl_indcr == car(x)) {
        check(x, 2, 2);
        return mkfix(cadr(x));
    } else if (S_kl_glob_env == car(x)) {
        check(x, 1, 1);
        return car(VMGLOB);
    } else if (S_end_of_file_p == car(x)) {
        check(x, 2, 2);
        return EOTOBJ == cadr(x) ? S_t : NIL;
    } else if (S_read == car(x)) {
        check(x, 1, 1);
        return xread();
    } else if (S_prin1 == car(x)) {
        check(x, 2, 2);
        prin1(cadr(x));
        return cadr(x);
    } else if (S_princ == car(x)) {
        check(x, 2, 2);
        princ(cadr(x));
        return cadr(x);
    } else if (S_finish_output == car(x)) {
        check(x, 1, 1);
        flush();
        return NIL;
    } else if (S_read_byte == car(x)) {
        check(x, 1, 1);
        return read_byte(0);
    } else if (S_peek_byte == car(x)) {
        check(x, 1, 1);
        return read_byte(1);
    } else if (S_write_byte == car(x)) {
        check(x, 2, 2);
        return write_byte(cadr(x));
    } else if (S_terpri == car(x)) {
        check(x, 1, 1);
        nl();
        return S_t;
    } else if (S_load == car(x)) {
        check(x, 2, 2);
        if (!symbolp(cadr(x)) && !stringp(cadr(x))) wrongtype(x);
        s = stringp(cadr(x)) ? stringval(cadr(x)) : symstr(cadr(x));
        load(s);
        return S_t;
    } else if (S_input_file == car(x)) {
        check(x, 1, 2);
        return input_file(NIL == cdr(x) ? NIL : cadr(x));
    } else if (S_append_file == car(x)) {
        check(x, 1, 2);
        return output_file(NIL == cdr(x) ? NIL : cadr(x), 1);
    } else if (S_output_file == car(x)) {
        check(x, 1, 2);
        return output_file(NIL == cdr(x) ? NIL : cadr(x), 0);
    } else if (S_delete_file == car(x)) {
        check(x, 1, 2);
        return delete_file(cadr(x));
    } else if (S_esccontp == car(x)) {
        check(x, 2, 2);
        return esccontp(cadr(x)) ? S_t : NIL;
    } else if (S_error == car(x)) {
        check(x, 2, 3);
        if (!symbolp(cadr(x)) && !stringp(cadr(x))) wrongtype(x);
        s = symbolp(cadr(x)) ? symstr(cadr(x)) : (char *)string(cadr(x));
        if (NIL == cddr(x))
            error(s, UNDEF);
        else
            error(s, caddr(x));
        return UNDEF;
    } else if (S_gc == car(x)) {
        check(x, 1, 2);
        return user_gc(NIL == cdr(x) ? NIL : cadr(x));
    } else if (S_suspend == car(x)) {
        check(x, 2, 2);
        return suspend(cadr(x));
    } else if (S_halt == car(x)) {
        check(x, 1, 1);
        flush();
        exit(0);
    } else {
        syntax(x);
        return UNDEF;
    }
}

static void cklam(cell x) {
    cell p;

    check(x, 3, -1);
    for (p = cadr(x); !atomp(p); p = cdr(p))
        if (!symbolp(car(p))) syntax(x);
}

#define mkfun(x) (cons3(NIL, cons(Env, cdr(x)), ATOM | TFUN))

#define msave(n) (Mstack = cons3(n, Mstack, ATOM))

static cell munsave(void) {
    cell n;

    if (NIL == Mstack) fatal("mstack empty");
    n = car(Mstack);
    Mstack = cdr(Mstack);
    return n;
}

enum { MHALT = 0,
       MEXPR,
       MLIST,
       MBETA,
       MRETN,
       MAPPL,
       MPRED,
       MNOTP,
       MSETQ,
       MPROG };

static cell special(cell x, cell *pm) {
    if (S_quote == car(x)) {
        check(x, 2, 2);
        *pm = munsave();
        return cadr(x);
    } else if (S_if == car(x)) {
        check(x, 3, 4);
        msave(MPRED);
        *pm = MEXPR;
        save(cddr(x));
        return cadr(x);
    } else if (S_progn == car(x)) {
        *pm = MEXPR;
        if (NIL == cdr(x)) return NIL;
        if (NIL == cddr(x)) return cadr(x);
        msave(MPROG);
        save(cddr(x));
        return cadr(x);
    }
    if (S_ifnot == car(x)) {
        check(x, 3, 3);
        msave(MNOTP);
        *pm = MEXPR;
        save(caddr(x));
        return cadr(x);
    } else if (S_lambda == car(x)) {
        cklam(x);
        *pm = munsave();
        return mkfun(x);
    } else if (S_apply == car(x)) {
        check(x, 3, 3);
        msave(MAPPL);
        *pm = MEXPR;
        save(caddr(x));
        save(NIL);
        return cadr(x);
    } else if (S_setq == car(x) || S_define == car(x)) {
        check(x, 3, 3);
        if (!symbolp(cadr(x))) syntax(x);
        msave(MSETQ);
        *pm = MEXPR;
        save(cadr(x));
        return caddr(x);
    } else {
        syntax(x);
        return UNDEF;
    }
}

static void bindargs(cell v, cell a) {
    cell e, n;

    save(e = NIL);
    while (!atomp(v)) {
        if (NIL == a) error("too few args", Acc);
        n = cons(car(v), cons(car(a), NIL));
        e = cons(n, e);
        rplaca(Stack, e);
        v = cdr(v);
        a = cdr(a);
    }
    if (symbolp(v)) {
        n = cons(v, cons(a, NIL));
        e = cons(n, e);
        rplaca(Stack, e);
    } else if (a != NIL) {
        error("extra args", Acc);
    }
    Env = cons(e, Env);
    unsave(1);
}

static cell funapp(cell x) {
    Acc = x;
    if (!funp(car(x))) syntax(x);
    if (Mstack != NIL && MRETN == car(Mstack)) {
        Env = cadar(Acc);
        bindargs(caddar(Acc), cdr(Acc));
    } else {
        save(Env);
        Env = cadar(Acc);
        bindargs(caddar(Acc), cdr(Acc));
        msave(MRETN);
    }
    return cons(S_progn, cdddar(x));
}

static cell expand(cell x) {
    cell n, m, p;

    if (atomp(x)) return x;
    if (S_quote == car(x)) return x;
    n = symbolp(car(x)) ? findmac(car(x)) : UNDEF;
    if (n != UNDEF) {
        m = cons(cdr(x), NIL);
        m = cons(S_quote, m);
        m = cons(m, NIL);
        m = cons(cdr(n), m);
        m = cons(S_apply, m);
        save(m);
        n = eval(m);
        rplaca(Stack, n);
        n = expand(n);
        unsave(1);
        return n;
    }
    for (p = x; !atomp(p); p = cdr(p))
        ;
    if (symbolp(p)) return x;
    save(x);
    save(n = NIL);
    for (p = x; p != NIL; p = cdr(p)) {
        m = expand(car(p));
        n = cons(m, n);
        rplaca(Stack, n);
    }
    n = nrev(unsave(1));
    unsave(1);
    return n;
}

static int specialp(cell n) {
    return n == S_quote ||
           n == S_if ||
           n == S_progn ||
           n == S_ifnot ||
           n == S_lambda ||
           n == S_apply ||
           n == S_define ||
           n == S_setq;
}

static cell eval(cell x) {
    cell n, m;

    Acc = expand(x);
    msave(MHALT);
    m = MEXPR;
    while (0 == Error) {
        switch (m) {
            case MEXPR:
                if (symbolp(Acc)) {
                    n = car(lookup(Acc));
                    if (UNDEF == n)
                        error("undefined", Acc);
                    Acc = n;
                    m = munsave();
                } else if (atomp(Acc)) {
                    m = munsave();
                } else if (specialp(car(Acc))) {
                    m = MBETA;
                } else {
                    save(cdr(Acc));
                    Acc = car(Acc);
                    save(NIL);
                    msave(MLIST);
                }
                break;
            case MLIST:
                if (NIL == cadr(Stack)) {
                    Acc = nrev(cons(Acc, unsave(1)));
                    unsave(1);
                    m = MBETA;
                } else {
                    rplaca(Stack, cons(Acc, car(Stack)));
                    Acc = caadr(Stack);
                    rplaca(cdr(Stack), cdadr(Stack));
                    msave(m);
                    m = MEXPR;
                }
                break;
            case MAPPL:
                if (NIL == car(Stack)) {
                    rplaca(Stack, Acc);
                    Acc = cadr(Stack);
                    msave(MAPPL);
                    m = MEXPR;
                } else {
                    n = unsave(1);
                    unsave(1);
                    Acc = cons(n, Acc);
                    m = MBETA;
                }
                break;
            case MPRED:
                n = unsave(1);
                if (NIL == Acc)
                    Acc = NIL == cdr(n) ? NIL : cadr(n);
                else
                    Acc = car(n);
                m = MEXPR;
                break;
            case MNOTP:
                n = unsave(1);
                if (NIL == Acc) {
                    Acc = n;
                    m = MEXPR;
                } else {
                    m = munsave();
                }
                break;
            case MBETA:
                if (specialp(car(Acc))) {
                    Acc = special(Acc, &m);
                } else if (symbolp(car(Acc))) {
                    Acc = builtin(Acc);
                    m = munsave();
                } else if (cfnp(car(Acc))) {
                    if (cdr(Acc) != NIL) error("extra args", x);
                    Acc = run(car(Acc));
                    m = munsave();
                } else {
                    Acc = funapp(Acc);
                    m = MEXPR;
                }
                break;
            case MRETN:
                Env = unsave(1);
                m = munsave();
                break;
            case MSETQ:
                n = unsave(1);
                rplaca(lookup(n), Acc);
                m = munsave();
                break;
            case MPROG:
                if (NIL == cdar(Stack)) {
                    Acc = car(unsave(1));
                    m = MEXPR;
                } else {
                    Acc = caar(Stack);
                    rplaca(Stack, cdar(Stack));
                    msave(MPROG);
                    m = MEXPR;
                }
                break;
            case MHALT:
                return Acc;
        }
    }
    return NIL;
}

#endif /* BOOTSTRAP */

cell KL_features;

#define feature(x) \
    rplaca(KL_features, cons(addsym(x, UNDEF), car(KL_features)))

static void init(void) {
    Verbose_GC = 0;
    Debug_GC = 0;
    Tmpcar = NIL;
    Tmpcdr = NIL;
    Tmp = NIL;
    Parens = 0;
    Stack = NIL;
    Mstack = NIL;
    Istack = NIL;
    VMprog = NIL;
    VMenv = NIL;
    VMargs = NIL;
    Ostack = NIL;
    Error = 0;
    Error_obj = 0;
    Tracept = 0;
    memset(Trace, 0, 10 * sizeof(cell)); /* assert NIL == 0 */
    Env = NIL;
    Car[NIL] = NIL;
    Cdr[NIL] = NIL;
    Tag[NIL] = 0;
    Car[FRELIS] = NIL;
    Cdr[FRELIS] = NIL;
    Tag[FRELIS] = ATOM;
    Car[FREVEC] = NIL;
    Cdr[FREVEC] = NIL;
    Tag[FREVEC] = ATOM;
    Car[SYMLIS] = mkht();
    Cdr[SYMLIS] = NIL;
    Tag[SYMLIS] = 0;
    Car[GENSYM] = mkfix(0);
    Cdr[GENSYM] = NIL;
    Tag[GENSYM] = 0;
    Car[UNDEF] = NIL;
    Cdr[UNDEF] = NIL;
    Tag[UNDEF] = ATOM;
    Car[ERROR] = NIL;
    Cdr[ERROR] = NIL;
    Tag[ERROR] = 0;
    Car[START] = NIL;
    Cdr[START] = NIL;
    Tag[START] = 0;
    Car[VMGLOB] = NIL;
    Cdr[VMGLOB] = NIL;
    Tag[VMGLOB] = 0;
    Car[RESET] = NIL;
    Cdr[RESET] = NIL;
    Tag[RESET] = 0;
    Car[NULOBJ] = NIL;
    Cdr[NULOBJ] = NIL;
    Tag[NULOBJ] = 0;
    Car[EOTOBJ] = NIL;
    Cdr[EOTOBJ] = NIL;
    Tag[EOTOBJ] = 0;
    Car[CELLS] = NIL;
    Cdr[CELLS] = NIL;
    Tag[CELLS] = 0;
    redir_output(NULL, 0);
    redir_input(NULL);
    rplaca(NULOBJ, newvec(TSTR, 0, 0));
    rplaca(NULOBJ, cons(newvec(TBVE, 0, 0), car(NULOBJ)));
    rplaca(NULOBJ, cons(newvec(TVEC, 0, 0), car(NULOBJ)));
    S_t = addsym("t", SELF);
    S_dot = addsym("..", SELF);
    S_rparen = addsym(".)", SELF);
    S_apply = addsym("apply*", UNDEF);
    S_if = addsym("if", UNDEF);
    S_ifnot = addsym("ifnot", UNDEF);
    S_lambda = addsym("lambda", UNDEF);
    S_progn = addsym("progn", UNDEF);
    S_quote = addsym("quote", UNDEF);
    S_quasiquote = addsym("quasiquote", UNDEF);
    S_unquote = addsym("unquote", UNDEF);
    S_unquote_splice = addsym("unquote-splice", UNDEF);
    S_setq = addsym("setq", UNDEF);
    S_starstar = addsym("**", UNDEF);
    S_add1 = addsym("add1", SELF);
    S_append_file = addsym("append-file", SELF);
    S_atom = addsym("atom", SELF);
    S_bytevecp = addsym("bytevecp", SELF);
    S_car = addsym("car", SELF);
    S_cdr = addsym("cdr", SELF);
    S_cons = addsym("cons", SELF);
    S_define = addsym("define", SELF);
    S_delete_file = addsym("delete-file", SELF);
    S_difference = addsym("difference", SELF);
    S_divide = addsym("divide", SELF);
    S_end_of_file_p = addsym("end-of-file-p", SELF);
    S_eq = addsym("eq", SELF);
    S_eqv = addsym("eqv", SELF);
    S_error = addsym("error", SELF);
    S_esccontp = addsym("esccontp", SELF);
    S_finish_output = addsym("finish-output", SELF);
    S_fixnump = addsym("fixnump", SELF);
    S_functionp = addsym("functionp", SELF);
    S_halt = addsym("halt", SELF);
    S_gc = addsym("gc", SELF);
    S_greaterp = addsym("greaterp", SELF);
    S_input_file = addsym("input-file", SELF);
    S_kl_car = addsym("kl-car", SELF);
    S_kl_cdr = addsym("kl-cdr", SELF);
    S_kl_cons3 = addsym("kl-cons3", SELF);
    S_kl_crval = addsym("kl-crval", SELF);
    S_kl_glob_env = addsym("kl-glob-env", SELF);
    S_kl_indcr = addsym("kl-indcr", SELF);
    S_kl_rplacd = addsym("kl-rplacd", SELF);
    S_leftshift = addsym("leftshift", SELF);
    S_lessp = addsym("lessp", SELF);
    S_load = addsym("load", SELF);
    S_logand = addsym("logand", SELF);
    S_lognot = addsym("lognot", SELF);
    S_logor = addsym("logor", SELF);
    S_logxor = addsym("logxor", SELF);
    S_make_bytevec = addsym("make-bytevec", SELF);
    S_make_string = addsym("make-string", SELF);
    S_make_vector = addsym("make-vector", SELF);
    S_maksym = addsym("maksym", SELF);
    S_not_greaterp = addsym("not-greaterp", SELF);
    S_not_lessp = addsym("not-lessp", SELF);
    S_output_file = addsym("output-file", SELF);
    S_peek_byte = addsym("peek-byte", SELF);
    S_plus = addsym("plus", SELF);
    S_prin1 = addsym("prin1", SELF);
    S_princ = addsym("princ", SELF);
    S_read = addsym("read", SELF);
    S_read_byte = addsym("read-byte", SELF);
    S_rem = addsym("rem", SELF);
    S_rightshift = addsym("rightshift", SELF);
    S_rplaca = addsym("rplaca", SELF);
    S_rplacd = addsym("rplacd", SELF);
    S_samenamep = addsym("samenamep", SELF);
    S_stringp = addsym("stringp", SELF);
    S_sub1 = addsym("sub1", SELF);
    S_suspend = addsym("suspend", SELF);
    S_symbol_chars = addsym("symbol-chars", SELF);
    S_symbolp = addsym("symbolp", SELF);
    S_terpri = addsym("terpri", SELF);
    S_times = addsym("times", SELF);
    S_vector_copy = addsym("vector-copy", SELF);
    S_vectorp = addsym("vectorp", SELF);
    S_vref = addsym("vref", SELF);
    S_vset = addsym("vset", SELF);
    S_vsize = addsym("vsize", SELF);
    S_write_byte = addsym("write-byte", SELF);
    KL_glob_syms = lookup(addsym("kl-glob-syms", mkht()));
    KL_macro_env = lookup(addsym("kl-macro-env", NIL));
    KL_features = lookup(addsym("kl-features", NIL));
    rplaca(KL_features, NIL);
#ifdef KL24BIT
    feature("24-bit");
#endif /* KL24BIT */
#ifdef KLCLOCK
    feature("clock");
#endif
#ifdef KLREADKEY
    feature("read-key");
#endif
    rplaca(cdar(SYMLIS), S_samenamep); /* Change HT test of SYMLIS */
    rplaca(VMGLOB, nullvec());
    rplaca(TOPSYM, KL_glob_syms);
    rplaca(CELLS, cons(mkfix(NCELLS), NIL));
    rplacd(car(CELLS), mkfix(NVCELLS));
}

static void kbint(int x) { Error = 1; }

int main(int argc, char **argv) {
    init();
    fasload(argc > 1 ? argv[1] : "klisp");
    if (cfnp(car(START))) {
        run(car(START));
        fatal("VM exit");
    }
#ifndef BOOTSTRAP
    else {
        pr("? no entry point");
        nl();
        pr("? aborting");
        nl();
    }
#endif /* !BOOTSTRAP */
#ifdef BOOTSTRAP
    for (;;) {
        resetfiles();
        Parens = 0;
        Error = 0;
        Acc = Env = NIL;
        Stack = Mstack = NIL;
        pr("boot* ");
        flush();
        Acc = xread();
        if (Error) continue;
        if (EOTOBJ == Acc) break;
        Acc = eval(Acc);
        resetfiles();
        prin1(Acc);
        nl();
        rplaca(cdr(S_starstar), Acc);
    }
    nl();
#endif /* BOOTSTRAP */
    return 0;
}
