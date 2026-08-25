/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#define PATH_SEPARATOR '/'
#define __cdecl
namespace Os {
// Pci Path
extern const char *sysFsPciPathPrefix;
extern const char *pciDevicesDirectory;
// Proc Path
extern const char *sysFsProcPathPrefix;
// Cpu Path
extern const char *sysFsSystemCpuPathPrefix;
} // namespace Os
