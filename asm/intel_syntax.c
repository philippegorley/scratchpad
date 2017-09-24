#include <stdio.h>

#define INTEL_ASM(asm_lines, operands) do { \
    asm (".intel_syntax;"                   \
         asm_lines                          \
         ".att_syntax;"                     \
         operands);                         \
} while (0)

int main(void)
{
    int src = 1;
    int dst;

//    asm (".intel_syntax;"
//         "mov %0, %1;"
//         "add %0, 1;"
//         ".att_syntax;"
//         : "=r" (dst)
//         : "r" (src));

    INTEL_ASM("mov %0, %1;"
              "add %0, 1;",
              : "=r" (dst)
              : "r" (src));

    printf("%d\n", dst);
    return 0;
}

