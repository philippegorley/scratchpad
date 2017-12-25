#include <stdio.h>

int main(int argc, char **argv) {
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    printf("Size of %s is %lu\n", alphabet, sizeof(alphabet));
    return 0;
}
