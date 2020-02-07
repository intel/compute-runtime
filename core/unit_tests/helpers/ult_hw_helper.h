/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_helper.h"

namespace NEO {

template <typename GfxFamily>
struct UltPipeControlHelper : PipeControlHelper<GfxFamily> {
    using PipeControlHelper<GfxFamily>::getSizeForAdditonalSynchronization;

    static size_t getExpectedPipeControlCount(const HardwareInfo &hwInfo) {
        return (PipeControlHelper<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo) -
                PipeControlHelper<GfxFamily>::getSizeForAdditonalSynchronization(hwInfo)) /
               sizeof(typename GfxFamily::PIPE_CONTROL);
    }
};

} // namespace NEO
