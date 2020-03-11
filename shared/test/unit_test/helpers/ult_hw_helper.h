/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"

namespace NEO {

template <typename GfxFamily>
struct UltMemorySynchronizationCommands : MemorySynchronizationCommands<GfxFamily> {
    static size_t getExpectedPipeControlCount(const HardwareInfo &hwInfo) {
        return (MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo) -
                MemorySynchronizationCommands<GfxFamily>::getSizeForAdditonalSynchronization(hwInfo)) /
               sizeof(typename GfxFamily::PIPE_CONTROL);
    }
};

} // namespace NEO
