/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "common/gtsysinfo.h"
#include "igfxfmid.h"

namespace Os {
///////////////////////////////////////////////////////////////////////////////
// These options determine the Windows specific behavior for
// the runtime unit tests
///////////////////////////////////////////////////////////////////////////////
const char *frontEndDllName = "";
const char *igcDllName = "";
const char *gdiDllName = "";
const char *dxcoreDllName = "";
const char *testDllName = "test_dynamic_lib.dll";
const char *metricsLibraryDllName = "";
} // namespace Os
