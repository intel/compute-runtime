/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/addressing_mode_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/program/kernel_info.h"

namespace NEO::AddressingModeHelper {

bool failBuildProgramWithStatefulAccess(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();

    auto failBuildProgram = compilerProductHelper.failBuildProgramWithStatefulAccessPreference();
    if (NEO::debugManager.flags.FailBuildProgramWithStatefulAccess.get() != -1) {
        failBuildProgram = static_cast<bool>(NEO::debugManager.flags.FailBuildProgramWithStatefulAccess.get());
    }

    auto forceToStatelessRequired = compilerProductHelper.isForceToStatelessRequired();

    return failBuildProgram && forceToStatelessRequired;
}

inline bool argPointerIsStateful(const ArgDescriptor &arg) {
    return arg.is<NEO::ArgDescriptor::argTPointer>() &&
           (NEO::isValidOffset(arg.as<NEO::ArgDescPointer>().bindless) ||
            NEO::isValidOffset(arg.as<NEO::ArgDescPointer>().bindful));
}

bool containsStatefulAccess(const KernelDescriptor &kernelDescriptor, bool skipLastExplicitArg) {
    auto size = static_cast<int32_t>(kernelDescriptor.payloadMappings.explicitArgs.size());
    if (skipLastExplicitArg) {
        size--;
    }
    for (auto i = 0; i < size; i++) {
        if (argPointerIsStateful(kernelDescriptor.payloadMappings.explicitArgs[i])) {
            return true;
        }
    }
    return false;
}

bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos, bool skipLastExplicitArg) {
    for (const auto &kernelInfo : kernelInfos) {
        if (containsStatefulAccess(kernelInfo->kernelDescriptor, skipLastExplicitArg)) {
            return true;
        }
    }
    return false;
}

bool containsBindlessKernel(const std::vector<KernelInfo *> &kernelInfos) {
    for (const auto &kernelInfo : kernelInfos) {
        if (NEO::KernelDescriptor::isBindlessAddressingKernel(kernelInfo->kernelDescriptor)) {
            return true;
        }
    }
    return false;
}

} // namespace NEO::AddressingModeHelper
