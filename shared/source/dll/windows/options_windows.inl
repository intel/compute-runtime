/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "igc.opencl.h"

#include <cstdint>

namespace Os {

const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *gdiDllName = "gdi32.dll";
const char *dxcoreDllName = "dxcore.dll";

// Os specific Metrics Library name
#if _WIN64
const char *metricsLibraryDllName = "igdml64.dll";
#else
const char *metricsLibraryDllName = "igdml32.dll";
#endif
} // namespace Os

namespace ContextGroup {
uint32_t maxContextCount = 8;
}
