#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

// gcc -rdynamic -g3 backtrace.c

#define MAX_BT_LENGTH 10

void print_bt()
{
    void *bt[MAX_BT_LENGTH];
    size_t bt_len;
    char **strings;
    size_t i;

    bt_len = backtrace(bt, MAX_BT_LENGTH);
    strings = backtrace_symbols(bt, bt_len);

    printf("Backtrace contains %zu stack frames\n", bt_len);

    for(i = 0; i < bt_len; ++i)
        printf("%s\n", strings[i]);

    free(strings);
}

// add another call to the backtrace
void extra_func()
{
    print_bt();
}

int main()
{
    extra_func();
    return 0;
}
