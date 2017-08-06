#include <stdio.h>

int main(void)
{
    int a=10, b;
    __asm__ __volatile__ (
        "movl %1, %%eax;"
        "movl %%eax, %0;"
        :"=r"(b)        /* output */
        :"r"(a)         /* input */
        :"%eax"         /* clobbered register */
    ); 
    printf("b=%d\n", b);

    int foo = 10, bar = 15;
    __asm__ __volatile__(
        "addl  %%ebx,%%eax;"
        :"=a"(foo)
        :"a"(foo), "b"(bar)
    );
    printf("foo+bar=%d\n", foo);
    return 0;
}

