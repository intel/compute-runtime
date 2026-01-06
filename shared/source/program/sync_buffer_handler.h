/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <mutex>

namespace NEO {

class CommandStreamReceiver;
class Device;
class GraphicsAllocation;
class MemoryManager;

class SyncBufferHandler : NonCopyableAndNonMovableClass {
  public:
    ~SyncBufferHandler();

    SyncBufferHandler(Device &device);

    void makeResident(CommandStreamReceiver &csr);

    std::pair<GraphicsAllocation *, size_t> obtainAllocationAndOffset(size_t requiredSize);

  protected:
    void allocateNewBuffer();

    Device &device;
    MemoryManager &memoryManager;
    GraphicsAllocation *graphicsAllocation;
    const size_t bufferSize = 64 * MemoryConstants::kiloByte;
    size_t usedBufferSize = 0;
    std::mutex mutex;
};

static_assert(NEO::NonCopyableAndNonMovable<SyncBufferHandler>);

} // namespace NEO
