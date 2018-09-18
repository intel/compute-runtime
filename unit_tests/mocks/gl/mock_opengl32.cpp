/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>

extern "C" {

void *__stdcall wglGetProcAddress(const char *name) { return nullptr; }
}
