/*
 * Copyright (C) 2026 Intel Corporation
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

#include "shared/source/os_interface/gl_wrapper.h"

#include "GL/glext.h"
