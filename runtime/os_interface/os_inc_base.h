/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/os_library.h"

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
