/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"

#include <dlfcn.h>

namespace L0 {
int FirmwareUtilImp::fwUtilLoadFlags = RTLD_LAZY;
std::string FirmwareUtilImp::fwUtilLibraryName = "libigsc.so.0";
} // namespace L0