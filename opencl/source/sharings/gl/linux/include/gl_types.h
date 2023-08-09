/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <EGL/egl.h>

typedef void *GLDisplay;
typedef void *GLContext;
using GLType = uint32_t;
using GLFunctionType = decltype(&eglGetProcAddress);
