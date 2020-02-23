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
class RenderDispatcher : public Dispatcher {
  public:
    RenderDispatcher() = default;

    void dispatchPreemption(LinearStream &cmdBuffer) override;
    size_t getSizePreemption() override;

    void dispatchMonitorFence(LinearStream &cmdBuffer,
                              uint64_t gpuAddress,
                              uint64_t immediateData,
                              const HardwareInfo &hwInfo) override;
    size_t getSizeMonitorFence(const HardwareInfo &hwInfo) override;

    void dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo) override;
    size_t getSizeCacheFlush(const HardwareInfo &hwInfo) override;
};
} // namespace NEO
