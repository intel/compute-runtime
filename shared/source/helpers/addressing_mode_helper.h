/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>

namespace NEO {
struct KernelInfo;
struct RootDeviceEnvironment;

namespace AddressingModeHelper {
bool failBuildProgramWithStatefulAccess(const RootDeviceEnvironment &rootDeviceEnvironment);
bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos, bool skipLastExplicitArg);

} // namespace AddressingModeHelper
} // namespace NEO
