/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"

#include <cstdarg>
#include <cstdio>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace NEO {
namespace IoFunctions {
using fopenFuncPtr = FILE *(*)(const char *, const char *);
using vfprintfFuncPtr = int (*)(FILE *, char const *const formatStr, va_list arg);
using fcloseFuncPtr = int (*)(FILE *);
using getenvFuncPtr = decltype(&getenv);
using fseekFuncPtr = int (*)(FILE *, long int, int);
using ftellFuncPtr = long int (*)(FILE *);
using rewindFuncPtr = decltype(&rewind);
using freadFuncPtr = size_t (*)(void *, size_t, size_t, FILE *);
using fwriteFuncPtr = decltype(&fwrite);
using fflushFuncPtr = decltype(&fflush);
using mkdirFuncPtr = int (*)(const char *);

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
extern mkdirFuncPtr mkdirPtr;

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

inline char *getEnvironmentVariable(const char *name) {

    char *environmentVariable = getenvPtr(name);

    if (strnlen_s(environmentVariable, CommonConstants::maxAllowedEnvVariableSize) < CommonConstants::maxAllowedEnvVariableSize) {
        return environmentVariable;
    }

    return nullptr;
}

#ifdef _WIN32
inline int makedir(const char *dirName) {
    return _mkdir(dirName);
}
#else
inline int makedir(const char *dirName) {
    return mkdir(dirName, 0777);
}
#endif

} // namespace IoFunctions
} // namespace NEO