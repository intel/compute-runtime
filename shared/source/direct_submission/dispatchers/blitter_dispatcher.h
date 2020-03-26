/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/dispatchers/dispatcher.h"

namespace NEO {

template <typename GfxFamily>
class BlitterDispatcher : public Dispatcher<GfxFamily> {
  public:
    static void dispatchPreemption(LinearStream &cmdBuffer);
    static size_t getSizePreemption();

    static void dispatchMonitorFence(LinearStream &cmdBuffer,
                                     uint64_t gpuAddress,
                                     uint64_t immediateData,
                                     const HardwareInfo &hwInfo);
    static size_t getSizeMonitorFence(const HardwareInfo &hwInfo);

    static void dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo);
    static size_t getSizeCacheFlush(const HardwareInfo &hwInfo);
};
} // namespace NEO
