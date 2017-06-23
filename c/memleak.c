#include <stdlib.h>

void function_which_allocates(void) {
    /* allocate an array of 45 floats */
    float *a = malloc(sizeof(float) * 45);
    a[4] = 4.5;
}

int main(void) {
    function_which_allocates();
    
    return 0;
}
