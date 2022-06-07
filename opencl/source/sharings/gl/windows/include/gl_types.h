/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>

#define OSAPI WINAPI

typedef uint32_t GLType;
typedef HDC GLDisplay;
typedef HGLRC GLContext;
