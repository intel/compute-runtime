/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <EGL/egl.h>

typedef void *GLDisplay;
typedef void *GLContext;
using GLType = uint32_t;
using GLFunctionType = decltype(&eglGetProcAddress);
