#include <stdio.h>
#include <unistd.h>
#include <time.h>

// source: http://zserge.com/blog/c-for-loop-tricks.html

#define BENCH_CLOCK() ((float)clock() * 1000000000L / CLOCKS_PER_SEC)

// _i will execute the printf but take the value of _iter (comma operator)
// the right side of the || will only be executed once the left side is no longer true (short circuit)
// the right side always evaluates to 0 (comma operator)
#define BENCH(_name, _iter)                                                                             \
    for (long _start_ns = BENCH_CLOCK(), _i = (printf("START: %15s\n", _name), _iter);                  \
         _i > 0 || (printf("BENCH: %15s: %f ns/op\n", _name, (BENCH_CLOCK() - _start_ns) / _iter), 0);  \
         _i--)

int main() {
    BENCH("sleep", 10L) {
        printf("Sleeping for another %ld seconds\n", _i);
        sleep(1);
    }
}
