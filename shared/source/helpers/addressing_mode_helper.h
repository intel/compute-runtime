/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

namespace NEO {
struct KernelInfo;

namespace AddressingModeHelper {
bool forceToStatelessNeeded(const std::string &options, const std::string &smallerThan4GbBuffersOnly, bool sharedSystemAllocationsAllowed);
bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos);

} // namespace AddressingModeHelper
} // namespace NEO
