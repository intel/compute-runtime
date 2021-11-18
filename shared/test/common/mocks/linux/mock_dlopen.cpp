/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdio>
#include <cstring>
#include <dlfcn.h>

void *(*dlopenFunc)(const char *filename, int flags) = nullptr;
char *(*dlerrorFunc)() = nullptr;

static char dlerrorString[][16] = {"denied", "fake"};
static int dlopenError = -1;

void *dlopen(const char *filename, int flags) {
    if (dlerrorFunc == nullptr) {
        dlerrorFunc = reinterpret_cast<decltype(dlerrorFunc)>(dlsym(RTLD_NEXT, "dlerror"));
    }
    if (dlopenFunc == nullptr) {
        dlopenFunc = reinterpret_cast<decltype(dlopenFunc)>(dlsym(RTLD_NEXT, "dlopen"));
    }

    dlopenError = -1;
    if (filename == nullptr ||
        strcmp(filename, "libtest_dynamic_lib.so") == 0) {
        return dlopenFunc(filename, flags);
    }
    if (filename[0] == '_') {
        dlopenError = 1;
        return nullptr;
    }
    dlopenError = 0;
    return nullptr;
}

char *dlerror() {
    if (dlerrorFunc == nullptr) {
        dlerrorFunc = reinterpret_cast<decltype(dlerrorFunc)>(dlsym(RTLD_NEXT, "dlerror"));
    }
    if (dlopenError >= 0 && dlopenError < 2) {
        return dlerrorString[dlopenError];
    }
    return dlerrorFunc();
}
