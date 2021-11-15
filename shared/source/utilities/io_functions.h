/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdarg>
#include <cstdio>
#include <stdlib.h>

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

extern fopenFuncPtr fopenPtr;
extern vfprintfFuncPtr vfprintfPtr;
extern fcloseFuncPtr fclosePtr;
extern getenvFuncPtr getenvPtr;
extern fseekFuncPtr fseekPtr;
extern ftellFuncPtr ftellPtr;
extern rewindFuncPtr rewindPtr;
extern freadFuncPtr freadPtr;
extern fwriteFuncPtr fwritePtr;

inline int fprintf(FILE *fileDesc, char const *const formatStr, ...) {
    va_list args;
    va_start(args, formatStr);
    int ret = IoFunctions::vfprintfPtr(fileDesc, formatStr, args);
    va_end(args);
    return ret;
}
} // namespace IoFunctions

} // namespace NEO
