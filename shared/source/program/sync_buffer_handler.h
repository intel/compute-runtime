/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/basic_math.h"

#include <mutex>

namespace NEO {

class CommandStreamReceiver;
class Context;
class Device;
class GraphicsAllocation;
class MemoryManager;
class Kernel;

class SyncBufferHandler {
  public:
    ~SyncBufferHandler();

    SyncBufferHandler(Device &device);

    void prepareForEnqueue(size_t workGroupsCount, Kernel &kernel, CommandStreamReceiver &csr);

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
