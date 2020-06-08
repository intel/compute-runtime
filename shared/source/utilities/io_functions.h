/*
 * Copyright (C) 2020 Intel Corporation
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

extern fopenFuncPtr fopenPtr;
extern vfprintfFuncPtr vfprintfPtr;
extern fcloseFuncPtr fclosePtr;
extern getenvFuncPtr getenvPtr;

inline int fprintf(FILE *fileDesc, char const *const formatStr, ...) {
    va_list args;
    va_start(args, formatStr);
    int ret = IoFunctions::vfprintfPtr(fileDesc, formatStr, args);
    va_end(args);
    return ret;
}
} // namespace IoFunctions

} // namespace NEO
