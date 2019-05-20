/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "External/Common/GmmLibDllName.h"
#include "igc.opencl.h"

namespace Os {

const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *gdiDllName = "gdi32.dll";
const char *gmmDllName = GMM_UMD_DLL;
const char *gmmEntryName = GMM_ENTRY_NAME;

// Os specific Metrics Library name
#if _WIN64
const char *metricsLibraryDllName = "igdml64.dll";
#else
const char *metricsLibraryDllName = "igdml32.dll";
#endif
} // namespace Os
