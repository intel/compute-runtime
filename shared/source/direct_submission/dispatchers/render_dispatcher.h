/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/dispatchers/dispatcher.h"

namespace NEO {

struct RootDeviceEnvironment;
class ProductHelper;

template <typename GfxFamily>
class RenderDispatcher : public Dispatcher<GfxFamily> {
  public:
    static void dispatchPreemption(LinearStream &cmdBuffer);
    static size_t getSizePreemption();

    static void dispatchMonitorFence(LinearStream &cmdBuffer,
                                     uint64_t gpuAddress,
                                     uint64_t immediateData,
                                     const RootDeviceEnvironment &rootDeviceEnvironment,
                                     bool useNotifyEnable,
                                     bool partitionedWorkload,
                                     bool dcFlushRequired);
    static size_t getSizeMonitorFence(const RootDeviceEnvironment &rootDeviceEnvironment);

    static void dispatchCacheFlush(LinearStream &cmdBuffer, const RootDeviceEnvironment &rootDeviceEnvironment, uint64_t address);
    static void dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeCacheFlush(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeTlbFlush(const RootDeviceEnvironment &rootDeviceEnvironment);
    static bool isMultiTileSynchronizationSupported() {
        return true;
    }
    static constexpr bool isCopy() {
        return false;
    }
};
} // namespace NEO
