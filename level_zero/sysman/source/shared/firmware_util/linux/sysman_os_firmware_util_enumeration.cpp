/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

#include <dlfcn.h>

namespace L0 {
namespace Sysman {
int FirmwareUtilImp::fwUtilLoadFlags = RTLD_LAZY;
// Try newer version first, fallback to older version
std::vector<std::string> FirmwareUtilImp::fwUtilLibraryNames = {"libigsc.so.1", "libigsc.so.0"};
} // namespace Sysman
} // namespace L0
