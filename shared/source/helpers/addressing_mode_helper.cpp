/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "addressing_mode_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/program/kernel_info.h"

namespace NEO::AddressingModeHelper {

bool failBuildProgramWithStatefulAccess(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();

    auto failBuildProgram = compilerProductHelper.failBuildProgramWithStatefulAccessPreference();
    if (NEO::DebugManager.flags.FailBuildProgramWithStatefulAccess.get() != -1) {
        failBuildProgram = static_cast<bool>(NEO::DebugManager.flags.FailBuildProgramWithStatefulAccess.get());
    }

    auto forceToStatelessRequired = compilerProductHelper.isForceToStatelessRequired();

    return failBuildProgram && forceToStatelessRequired;
}

bool containsStatefulAccess(const std::vector<KernelInfo *> &kernelInfos) {
    for (const auto &kernelInfo : kernelInfos) {
        for (const auto &arg : kernelInfo->kernelDescriptor.payloadMappings.explicitArgs) {
            auto isStatefulAccess = arg.is<NEO::ArgDescriptor::ArgTPointer>() &&
                                    (NEO::isValidOffset(arg.as<NEO::ArgDescPointer>().bindless) ||
                                     NEO::isValidOffset(arg.as<NEO::ArgDescPointer>().bindful));
            if (isStatefulAccess) {
                return true;
            }
        }
    }
    return false;
}

} // namespace NEO::AddressingModeHelper
