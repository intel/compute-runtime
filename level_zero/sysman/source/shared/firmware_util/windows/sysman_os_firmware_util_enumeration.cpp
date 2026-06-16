/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

namespace L0 {
namespace Sysman {
// Windows currently uses single DLL name, but structured as vector for consistency
std::vector<std::string> FirmwareUtilImp::fwUtilLibraryNames = {"igsc.dll"};
int FirmwareUtilImp::fwUtilLoadFlags{};
} // namespace Sysman
} // namespace L0
