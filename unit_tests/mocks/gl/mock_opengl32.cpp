/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string.h>
#include "unit_tests/helpers/windows/mock_function.h"

extern "C" {
void *__stdcall wglGetProcAddress(const char *name) { return nullptr; }
void *__stdcall mockLoader(const char *name) {
    if (strcmp(name, "realFunction") == 0) {
        return *realFunction;
    }
    return nullptr;
}
}
