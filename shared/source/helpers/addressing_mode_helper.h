/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>

namespace NEO {
struct KernelDescriptor;
struct KernelInfo;
struct RootDeviceEnvironment;

namespace AddressingModeHelper {
bool failBuildProgramWithBufferStatefulAccess(const RootDeviceEnvironment &rootDeviceEnvironment);
bool containsBufferStatefulAccess(const KernelDescriptor &kernelDescriptor, bool skipLastExplicitArg);
bool containsBufferStatefulAccess(const std::vector<KernelInfo *> &kernelInfos, bool skipLastExplicitArg);
bool containsStatefulAccess(const KernelDescriptor &kernelDescriptor);
bool containsBindlessKernel(const std::vector<KernelInfo *> &kernelInfos);

} // namespace AddressingModeHelper
} // namespace NEO
