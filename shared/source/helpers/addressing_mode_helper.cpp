/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/addressing_mode_helper.h"

#include "shared/source/compiler_interface/compiler_options/compiler_options_base.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/program/kernel_info.h"

namespace NEO::AddressingModeHelper {

bool forceToStatelessNeeded(const std::string &options, const std::string &smallerThan4GbBuffersOnlyOption, bool sharedSystemAllocationsAllowed) {
    auto preferStateful = false;
    if (NEO::CompilerOptions::contains(options, smallerThan4GbBuffersOnlyOption)) {
        preferStateful = true;
    }
    if (NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.get() != -1) {
        preferStateful = static_cast<bool>(NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.get());
    }
    auto forceStateless = !preferStateful && sharedSystemAllocationsAllowed;
    return forceStateless;
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
