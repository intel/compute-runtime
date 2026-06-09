/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

std::unique_ptr<ReleaseHelper> ReleaseHelper::create(HardwareIpVersion hardwareIpVersion) {

    auto architecture = hardwareIpVersion.architecture;
    auto release = hardwareIpVersion.release;
    if (releaseHelperFactory[architecture] == nullptr || releaseHelperFactory[architecture][release] == nullptr) {
        return {nullptr};
    }
    auto createFunction = releaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

std::pair<bool, bool> ReleaseHelper::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isExtendedWARequired = isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, isRcs);
    if (debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }
    return {isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired(),
            isExtendedWARequired};
}

} // namespace NEO
