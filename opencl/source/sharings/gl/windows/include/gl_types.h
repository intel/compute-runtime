/*
 * Copyright (C) 2020-2023 Intel Corporation
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