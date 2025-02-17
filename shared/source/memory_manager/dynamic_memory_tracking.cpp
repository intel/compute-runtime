/*
 * Copyright (C) 2022-2025 Intel Corporation
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
#include <cxxabi.h>
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
    void *addresses[backtraceBufferSize];
    char **functions;
    auto pointersCount = backtrace(addresses, backtraceBufferSize);
    functions = backtrace_symbols(addresses, pointersCount);

    printf("\n backtrace collected -- START --\n");

    for (int symbolId = 0; symbolId < pointersCount; symbolId++) {
        Dl_info info;
        dladdr(addresses[symbolId], &info);
        char *realname;
        int status;
        realname = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
        if (realname) {
            printf("%s %s\n", info.dli_fname, realname);
        } else {
            printf("%s %s\n", functions[symbolId], info.dli_sname);
        }
        free(realname);
    }

    printf(" backtrace collected -- END --");

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
