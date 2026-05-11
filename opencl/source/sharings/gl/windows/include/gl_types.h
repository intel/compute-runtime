/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>

#define OSAPI WINAPI

using GLType = uint32_t;
using GLDisplay = HDC;
using GLContext = HGLRC;
using GLFunctionType = PROC(__stdcall *)(LPCSTR arg1);

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#endif
#include "GL/gl.h"
#include "GL/glext.h"
#if __clang__
#pragma clang diagnostic pop
#endif
