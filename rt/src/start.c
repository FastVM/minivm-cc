
#if !defined(NULL)
#define NULL ({(void*)0;})
#endif

int main(int argc, char **argv);

void _start() {
    main(0, NULL);
}
