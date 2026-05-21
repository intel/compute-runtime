/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

class Device;
class GraphicsAllocation;
class LinearStream;
class MemoryManager;

class FrontEndController : NonCopyableAndNonMovableClass {
  public:
    FrontEndController(Device *device);
    ~FrontEndController();

    bool initialize();

    size_t getFrontEndAllocationSize() const;
    GraphicsAllocation *getFrontEndStreamAllocation() const { return frontEndStream; }
    GraphicsAllocation *getFrontEndDataAllocation() const { return frontEndData; }

    uint32_t getDataSize() const { return dataSize; }
    uint32_t getStreamSize() const { return streamSize; }

  protected:
    Device *device = nullptr;
    MemoryManager *memoryManager = nullptr;
    GraphicsAllocation *frontEndStream = nullptr;
    GraphicsAllocation *frontEndData = nullptr;
    uint32_t dataSize = 0;
    uint32_t streamSize = 0;
};

} // namespace NEO
