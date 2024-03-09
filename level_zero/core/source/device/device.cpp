/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"

namespace L0 {

uint32_t Device::getRootDeviceIndex() const {
    return neoDevice->getRootDeviceIndex();
}

NEO::DebuggerL0 *Device::getL0Debugger() {
    return getNEODevice()->getL0Debugger();
}

NEO::TagAllocatorBase *Device::getDeviceInOrderCounterAllocator() {
    if (!deviceInOrderCounterAllocator.get()) {
        std::unique_lock<std::mutex> lock(inOrderAllocatorMutex);

        if (!deviceInOrderCounterAllocator.get()) {
            using NodeT = typename NEO::DeviceAllocNodeType<true>;
            RootDeviceIndicesContainer rootDeviceIndices = {getRootDeviceIndex()};

            const size_t maxPartitionCount = getNEODevice()->getDeviceBitfield().count();

            const size_t nodeSize = sizeof(uint64_t) * maxPartitionCount * 2; // Multiplied by 2 to handle 32b overflow

            DEBUG_BREAK_IF(alignUp(nodeSize, MemoryConstants::cacheLineSize) * NodeT::defaultAllocatorTagCount > MemoryConstants::pageSize64k);

            deviceInOrderCounterAllocator = std::make_unique<NEO::TagAllocator<NodeT>>(rootDeviceIndices, neoDevice->getMemoryManager(), NodeT::defaultAllocatorTagCount,
                                                                                       MemoryConstants::cacheLineSize, nodeSize, false, neoDevice->getDeviceBitfield());
        }
    }

    return deviceInOrderCounterAllocator.get();
}

} // namespace L0