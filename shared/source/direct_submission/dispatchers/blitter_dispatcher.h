/*
 * Copyright (C) 2020-2025 Intel Corporation
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
class BlitterDispatcher : public Dispatcher<GfxFamily> {
  public:
    static void dispatchPreemption(LinearStream &cmdBuffer);
    static size_t getSizePreemption();

    static void dispatchMonitorFence(LinearStream &cmdBuffer,
                                     uint64_t gpuAddress,
                                     uint64_t immediateData,
                                     const RootDeviceEnvironment &rootDeviceEnvironment,
                                     bool partitionedWorkload,
                                     bool dcFlushRequired,
                                     bool notifyKmd);
    static size_t getSizeMonitorFence(const RootDeviceEnvironment &rootDeviceEnvironment);

    static void dispatchCacheFlush(LinearStream &cmdBuffer, const RootDeviceEnvironment &rootDeviceEnvironment, uint64_t address);
    static void dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeCacheFlush(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSizeTlbFlush(const RootDeviceEnvironment &rootDeviceEnvironment);
    static bool isMultiTileSynchronizationSupported() {
        return false;
    }
    static constexpr bool isCopy() {
        return true;
    }
};
} // namespace NEO
