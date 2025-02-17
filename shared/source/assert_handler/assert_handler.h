/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
#include <mutex>
namespace NEO {
class Device;
class GraphicsAllocation;

#pragma pack(1)

struct AssertBufferHeader {
    uint32_t size = 0;
    uint32_t flags = 0;
    uint32_t begin = 0;
};

static_assert(sizeof(AssertBufferHeader) == 3u * sizeof(uint32_t));

#pragma pack()

class AssertHandler : NonCopyableAndNonMovableClass {
  public:
    AssertHandler(Device *device);
    MOCKABLE_VIRTUAL ~AssertHandler();

    GraphicsAllocation *getAssertBuffer() const {
        return assertBuffer;
    }

    bool checkAssert() const;
    MOCKABLE_VIRTUAL void printAssertAndAbort();

  protected:
    static constexpr size_t assertBufferSize = MemoryConstants::pageSize64k;
    void printMessage() const;

    std::mutex mtx;
    const Device *device = nullptr;
    GraphicsAllocation *assertBuffer = nullptr;
};
} // namespace NEO
