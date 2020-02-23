/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/spinlock.h"

#include <memory>
#include <unordered_map>

namespace NEO {
class SVMAllocsManager;

class PageFaultManager : public NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<PageFaultManager> create();

    virtual ~PageFaultManager() = default;

    void moveAllocationToGpuDomain(void *ptr);
    void moveAllocationsWithinUMAllocsManagerToGpuDomain(SVMAllocsManager *unifiedMemoryManager);
    void insertAllocation(void *ptr, size_t size, SVMAllocsManager *unifiedMemoryManager, void *cmdQ);
    void removeAllocation(void *ptr);

  protected:
    struct PageFaultData {
        size_t size;
        SVMAllocsManager *unifiedMemoryManager;
        void *cmdQ;
        bool isInGpuDomain;
    };

    virtual void allowCPUMemoryAccess(void *ptr, size_t size) = 0;
    virtual void protectCPUMemoryAccess(void *ptr, size_t size) = 0;

    MOCKABLE_VIRTUAL bool verifyPageFault(void *ptr);
    MOCKABLE_VIRTUAL void transferToCpu(void *ptr, size_t size, void *cmdQ);
    MOCKABLE_VIRTUAL void transferToGpu(void *ptr, void *cmdQ);
    MOCKABLE_VIRTUAL void setAubWritable(bool writable, void *ptr, SVMAllocsManager *unifiedMemoryManager);

    std::unordered_map<void *, PageFaultData> memoryData;
    SpinLock mtx;
};
} // namespace NEO
