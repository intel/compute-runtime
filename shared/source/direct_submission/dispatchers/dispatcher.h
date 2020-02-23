/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NEO {

struct HardwareInfo;
class LinearStream;

class Dispatcher {
  public:
    Dispatcher() = default;
    virtual ~Dispatcher() = default;

    virtual void dispatchPreemption(LinearStream &cmdBuffer) = 0;
    virtual size_t getSizePreemption() = 0;

    virtual void dispatchMonitorFence(LinearStream &cmdBuffer,
                                      uint64_t gpuAddress,
                                      uint64_t immediateData,
                                      const HardwareInfo &hwInfo) = 0;
    virtual size_t getSizeMonitorFence(const HardwareInfo &hwInfo) = 0;

    virtual void dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo) = 0;
    virtual size_t getSizeCacheFlush(const HardwareInfo &hwInfo) = 0;
};
} // namespace NEO
