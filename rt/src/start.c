
#if !defined(NULL)
#define NULL ((void *)0)
#endif

void ta_init(void);
int main(int argc, char **argv);

int _start() {
    ta_init();
    return main(0, NULL);
}
