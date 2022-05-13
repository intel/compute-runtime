/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>

namespace NEO {
struct KernelInfo;
struct HardwareInfo;

namespace AddressingModeHelper {
bool failBuildProgramWithStatefulAccess(const HardwareInfo &hwInfo);
bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos);

} // namespace AddressingModeHelper
} // namespace NEO
