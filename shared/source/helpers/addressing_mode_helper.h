/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <limits>
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
template <typename... Ts>
bool isAnyValueWiderThan32bit(Ts... args) {
    static_assert(((std::is_integral_v<Ts>)&&...));
    static_assert(((!std::is_signed_v<Ts>)&&...));

    constexpr auto u32Max = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    return ((static_cast<uint64_t>(args) > u32Max) || ...);
}

} // namespace AddressingModeHelper
} // namespace NEO
