/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"

namespace L0 {
std::vector<std::string> FirmwareUtilImp::fwUtilLibraryNames = {"igsc.dll"};
int FirmwareUtilImp::fwUtilLoadFlags{};
} // namespace L0
