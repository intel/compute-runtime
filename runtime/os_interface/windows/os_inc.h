/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#define PATH_SEPARATOR '\\'

namespace OCLRT {
class PerfProfiler;
extern __declspec(thread) PerfProfiler *gPerfProfiler;
}; // namespace OCLRT
// For now we need to keep this file clean of OS specific #includes.
// Only issues to address portability should be covered here.

namespace Os {
// OS GDI name
extern const char *gdiDllName;
}; // namespace Os
