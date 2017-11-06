# Cross compile ffmpeg for android

1. $ANDROID_NDK/build/tools/make_standalone_toolchain.py --arch=arm --api=18 --install-dir /tmp/my-android-toolchain
2. PATH="/tmp/my-android-toolchain/bin:${PATH}" ./configure --arch=armv7 --cpu=armv7-a --enable-cross-compile --target-os=android --cross-prefix=arm-linux-androideabi- --cc=arm-linux-androideabi-clang
3. PATH="/tmp/my-android-toolchain/bin:${PATH}" make -j4


# Linker

- C++ needs `extern "C"` to include C headers
- C++ supports function overloading, C doesn't; this can lead to undefined references if not careful


# LD_PRELOAD

Replace function with own implementation.

## Simple example

unrandom.c:
```
int rand(){
    return 42;
}
```
Compile with `gcc -shared -fPIC unrandom.c -o unrandom.so`
Run program using `rand()` with env var `LD_PRELOAD=unrandom.so`

## Calling original function

Define _GNU_SOURCE and use dlsym()

```
#define _GNU_SOURCE

#include <stdio.h>
#include <sys/mman.h>
#include <dlfcn.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    printf("In our own implementation, calling original mmap now...\n");
    void *(*original_mmap)(void*, size_t, int, int, int, off_t);
    original_mmap = dlsym(RTLD_NEXT, "mmap");
    (*original_mmap)(addr, length, prot, flags, fd, offset);
}
```
