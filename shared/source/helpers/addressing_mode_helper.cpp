/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "addressing_mode_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/program/kernel_info.h"

namespace NEO::AddressingModeHelper {

bool failBuildProgramWithStatefulAccess(const HardwareInfo &hwInfo) {
    auto failBuildProgram = false;
    if (NEO::DebugManager.flags.FailBuildProgramWithStatefulAccess.get() != -1) {
        failBuildProgram = static_cast<bool>(NEO::DebugManager.flags.FailBuildProgramWithStatefulAccess.get());
    }

    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto forceToStatelessRequired = compilerHwInfoConfig.isForceToStatelessRequired();

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
