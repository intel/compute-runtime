/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdarg>
#include <cstdio>

namespace NEO {
namespace ResLog {
using fopenFuncPtr = FILE *(*)(const char *, const char *);
using vfprintfFuncPtr = int (*)(FILE *, char const *const formatStr, va_list arg);
using fcloseFuncPtr = int (*)(FILE *);

extern fopenFuncPtr fopenPtr;
extern vfprintfFuncPtr vfprintfPtr;
extern fcloseFuncPtr fclosePtr;
} // namespace ResLog

#if defined(_RELEASE_INTERNAL) || (_DEBUG)
constexpr bool residencyLoggingAvailable = true;
#else
constexpr bool residencyLoggingAvailable = false;
#endif
} // namespace NEO
