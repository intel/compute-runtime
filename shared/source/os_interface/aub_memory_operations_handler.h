/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
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

    bool isAubWritable(GraphicsAllocation &graphicsAllocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable) const;
    void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable);
    void setAddressWidth(uint32_t addressWidth) { this->addressWidth = addressWidth; }

  protected:
    DeviceBitfield getMemoryBanksBitfield(GraphicsAllocation *allocation, DeviceBitfield contextDeviceBitfield, bool isMultiOsContextCapable) const;
    MemoryOperationsStatus makeResidentWithinDevice(ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, bool forcePagingFence, DeviceBitfield deviceBitfield, bool isMultiOsContextCapable);
    [[nodiscard]] MOCKABLE_VIRTUAL std::unique_lock<SpinLock> acquireLock(SpinLock &lock) {
        return std::unique_lock<SpinLock>{lock};
    }
    uint64_t decanonizeAddress(uint64_t address) const {
        return addressWidth > 0 ? (address & maxNBitValue(addressWidth)) : address;
    }
    aub_stream::AubManager *aubManager = nullptr;
    std::vector<GraphicsAllocation *> residentAllocations;
    SpinLock resourcesLock;
    uint32_t addressWidth = 0;
};
} // namespace NEO
