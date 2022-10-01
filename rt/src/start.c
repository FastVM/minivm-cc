
#if !defined(NULL)
#define NULL ({ (void *)0; })
#endif

int main(int argc, char **argv);

int _start() {
    return main(0, NULL);
}
