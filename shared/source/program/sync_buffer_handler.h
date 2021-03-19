/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <mutex>

namespace NEO {

class CommandStreamReceiver;
class Context;
class Device;
class GraphicsAllocation;
class Kernel;
class MemoryManager;

class SyncBufferHandler {
  public:
    ~SyncBufferHandler();

    SyncBufferHandler(Device &device);

    template <typename KernelT>
    void prepareForEnqueue(size_t workGroupsCount, KernelT &kernel);
    void makeResident(CommandStreamReceiver &csr);

  protected:
    void allocateNewBuffer();

    Device &device;
    MemoryManager &memoryManager;
    GraphicsAllocation *graphicsAllocation;
    const size_t bufferSize = 64 * KB;
    size_t usedBufferSize = 0;
    std::mutex mutex;
};

} // namespace NEO
