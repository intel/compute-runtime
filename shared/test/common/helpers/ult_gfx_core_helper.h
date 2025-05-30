/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
struct UltMemorySynchronizationCommands : MemorySynchronizationCommands<GfxFamily> {
    static size_t getExpectedPipeControlCount(const RootDeviceEnvironment &rootDeviceEnvironment) {
        return (MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData) -
                MemorySynchronizationCommands<GfxFamily>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment)) /
               sizeof(typename GfxFamily::PIPE_CONTROL);
    }
};

} // namespace NEO
