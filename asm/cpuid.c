#include <stdio.h>

static inline void cpuid(unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
    asm volatile("cpuid"
                 : "=a" (*eax)
                 , "=b" (*ebx)
                 , "=c" (*ecx)
                 , "=d" (*edx)
                 : "0" (*eax)
                 , "2" (*ecx));
}

int main(int argc, char **argv)
{
    unsigned eax, ebx, ecx, edx;

    // different values in eax return different things, see https://en.wikipedia.org/wiki/CPUID
    eax = 1; // processor info and features
    cpuid(&eax, &ebx, &ecx, &edx);

    printf("Stepping %d\n", eax & 0xF);
    printf("Model %d\n", (eax >> 4) & 0xF);
    printf("Family %d\n", (eax >> 8) & 0xF);
    printf("Processor type %d\n", (eax >> 12) & 0x3);
    printf("Extended model %d\n", (eax >> 16) & 0xF);
    printf("Extended family %d\n", (eax >> 20) & 0xFF);
}
