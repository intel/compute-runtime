/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

#include <dlfcn.h>

namespace L0 {
namespace Sysman {
int FirmwareUtilImp::fwUtilLoadFlags = RTLD_LAZY;
std::string FirmwareUtilImp::fwUtilLibraryName = "libigsc.so.0";
} // namespace Sysman
} // namespace L0