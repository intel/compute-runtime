/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "igc.opencl.h"
#include "External/Common/GmmLibDllName.h"

namespace Os {

const char *frontEndDllName = FCL_LIBRARY_NAME;
const char *igcDllName = IGC_LIBRARY_NAME;
const char *gdiDllName = "gdi32.dll";
const char *gmmDllName = GMM_UMD_DLL;
const char *gmmEntryName = GMM_ENTRY_NAME;
} // namespace Os
