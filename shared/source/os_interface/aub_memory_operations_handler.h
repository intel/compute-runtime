/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/spinlock.h"

#include "aubstream/aub_manager.h"

#include <mutex>
#include <vector>

namespace NEO {

class AubMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    AubMemoryOperationsHandler(aub_stream::AubManager *aubManager);
    ~AubMemoryOperationsHandler() override = default;

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override;
    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) override;

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override;
    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override;

    void processFlushResidency(CommandStreamReceiver *csr) override;

    void setAubManager(aub_stream::AubManager *aubManager);

    bool isAubWritable(GraphicsAllocation &graphicsAllocation, Device *device) const;
    void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation, Device *device);

  protected:
    DeviceBitfield getMemoryBanksBitfield(GraphicsAllocation *allocation, Device *device) const;
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }
    aub_stream::AubManager *aubManager = nullptr;
    std::vector<GraphicsAllocation *> residentAllocations;
    SpinLock resourcesLock;
};
} // namespace NEO
