/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_library.h"

namespace Os {
// Compiler library names
extern const char *frontEndDllName;
extern const char *igcDllName;
extern const char *testDllName;

// OS specific directory separator
extern const char *fileSeparator;

// Os specific Metrics Library name
extern const char *metricsLibraryDllName;
}; // namespace Os
