/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/abort.h"
#include "shared/source/helpers/preprocessor.h"

#define UNRECOVERABLE_IF(expression)                             \
    if (expression) {                                            \
        NEO::abortUnrecoverable(__LINE__, NEO_SOURCE_FILE_PATH); \
    }

#define UNREACHABLE(...) std::abort()

#ifndef DEBUG_BREAK_IF
#ifdef _DEBUG
#define DEBUG_BREAK_IF(expression)           \
                                             \
    if (expression) {                        \
        NEO::debugBreak(__LINE__, __FILE__); \
    }
#else
#define DEBUG_BREAK_IF(expression) (void)0
#endif // _DEBUG
#endif // !DEBUG_BREAK_IF

namespace NEO {
void debugBreak(int line, const char *file);
[[noreturn]] void abortUnrecoverable(int line, const char *file);
} // namespace NEO
