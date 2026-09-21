/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/release_helper/release_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

std::unique_ptr<ReleaseHelper> ReleaseHelper::create(HardwareIpVersion hardwareIpVersion) {

    auto architecture = hardwareIpVersion.architecture;
    auto release = hardwareIpVersion.release;
    UNRECOVERABLE_IF(releaseHelperFactory[architecture] == nullptr || releaseHelperFactory[architecture][release] == nullptr);
    auto createFunction = releaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

uint64_t ReleaseHelper::overrideSystemMemoryPatIndex(uint64_t patIndex) const {
    if (debugManager.flags.EnableOverrideToPat19ForSystemMemory.get() != -1) {
        return debugManager.flags.EnableOverrideToPat19ForSystemMemory.get() == 1 ? 19u : patIndex;
    }

    return this->overrideSystemMemoryPatIndexBase(patIndex);
}

} // namespace NEO
