/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdarg>
#include <cstdio>
#include <stdlib.h>
#include <string.h>

namespace NEO {
namespace IoFunctions {
using fopenFuncPtr = FILE *(*)(const char *, const char *);
using vfprintfFuncPtr = int (*)(FILE *, char const *const formatStr, va_list arg);
using fcloseFuncPtr = int (*)(FILE *);
using getenvFuncPtr = decltype(&getenv);
using fseekFuncPtr = decltype(&fseek);
using ftellFuncPtr = decltype(&ftell);
using rewindFuncPtr = decltype(&rewind);
using freadFuncPtr = decltype(&fread);
using fwriteFuncPtr = decltype(&fwrite);
using fflushFuncPtr = decltype(&fflush);

extern fopenFuncPtr fopenPtr;
extern vfprintfFuncPtr vfprintfPtr;
extern fcloseFuncPtr fclosePtr;
extern getenvFuncPtr getenvPtr;
extern fseekFuncPtr fseekPtr;
extern ftellFuncPtr ftellPtr;
extern rewindFuncPtr rewindPtr;
extern freadFuncPtr freadPtr;
extern fwriteFuncPtr fwritePtr;
extern fflushFuncPtr fflushPtr;

inline int fprintf(FILE *fileDesc, char const *const formatStr, ...) {
    va_list args;
    va_start(args, formatStr);
    int ret = IoFunctions::vfprintfPtr(fileDesc, formatStr, args);
    va_end(args);
    return ret;
}

inline bool getEnvToBool(const char *name) {
    const char *env = getenvPtr(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}
} // namespace IoFunctions

} // namespace NEO
