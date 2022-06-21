
#if !defined(NULL)
#define NULL ({(void*)0;})
#endif

void ta_init(void);

int main(int argc, char **argv);

void _start() {
    ta_init();
    main(0, NULL);
}
