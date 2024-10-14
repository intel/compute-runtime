/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

namespace L0 {
namespace Sysman {
std::string FirmwareUtilImp::fwUtilLibraryName = "igsc.dll";
int FirmwareUtilImp::fwUtilLoadFlags{};
} // namespace Sysman
} // namespace L0