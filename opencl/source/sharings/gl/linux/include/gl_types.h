/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <EGL/egl.h>

using GLDisplay = EGLDisplay;
using GLContext = EGLContext;
using GLType = uint32_t;
using GLFunctionType = decltype(&eglGetProcAddress);
