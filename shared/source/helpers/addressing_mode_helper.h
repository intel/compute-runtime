/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

namespace NEO {
struct KernelInfo;
struct HardwareInfo;

namespace AddressingModeHelper {
bool forceToStatelessNeeded(const std::string &options, const std::string &smallerThan4GbBuffersOnlyOption, const HardwareInfo &hwInfo);
bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos);

} // namespace AddressingModeHelper
} // namespace NEO
