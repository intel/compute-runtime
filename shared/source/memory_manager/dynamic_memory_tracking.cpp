/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef ENABLE_DYNAMIC_MEMORY_TRACKING

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <exception>
#include <execinfo.h>
#include <iostream>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const bool enableBacktrace = true;
constexpr u_int32_t backtraceBufferSize = 256;

void collectBacktrace() {
    void *addressess[backtraceBufferSize];
    char **functions;
    auto pointersCount = backtrace(addressess, backtraceBufferSize);
    functions = backtrace_symbols(addressess, pointersCount);

    printf("\n backtrace collected -- START --");

    for (int symbolId = 0; symbolId < pointersCount; symbolId++) {
        Dl_info info;
        dladdr(addressess[symbolId], &info);
        printf("%s %s\n", functions[symbolId], info.dli_sname);
    }

    printf("\n backtrace collected -- END --");

    free(functions);
}

void *operator new(size_t size) {
    if (enableBacktrace) {
        collectBacktrace();
    }

    return malloc(size);
}

void *operator new(size_t size, const std::nothrow_t &) noexcept {
    if (enableBacktrace) {
        collectBacktrace();
    }

    return malloc(size);
}

void *operator new[](size_t size) {
    if (enableBacktrace) {
        collectBacktrace();
    }

    return malloc(size);
}

void *operator new[](size_t size, const std::nothrow_t &t) noexcept {
    if (enableBacktrace) {
        collectBacktrace();
    }

    return malloc(size);
}

void operator delete(void *p) noexcept {
    free(p);
}

void operator delete[](void *p) noexcept {
    free(p);
}
void operator delete(void *p, size_t size) noexcept {
    free(p);
}

void operator delete[](void *p, size_t size) noexcept {
    free(p);
}

#endif
