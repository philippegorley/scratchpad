#include <stdio.h>
#include <stdlib.h>

typedef struct Sample {
    int i;
    char *s;
} Sample;

int main(int argc, char *argv[])
{
    int sz = 3;
    Sample *arr = malloc(sz * sizeof(Sample));
    for (int i = 0; i < sz; ++i) {
        Sample s;
        s.i = i + 1;
        s.s = malloc(10);
        sprintf(s.s, "%s%d", "Index:", i);
        arr[i] = s;
    }
    Sample *s = &arr[1];
    printf("s->s:%s vs s[-1].s:%s\n", s->s, s[-1].s);

    for (int i = 0; i < sz; ++i)
        free(arr[i].s);
    free(arr);
    return 0;
}
