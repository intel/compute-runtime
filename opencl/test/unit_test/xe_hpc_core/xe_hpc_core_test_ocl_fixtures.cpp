/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {

void HwHelperTestsXeHpcCore::setupDeviceIdAndRevision(HardwareInfo *hwInfo, ClDevice &clDevice) {
    auto deviceHwInfo = clDevice.getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    deviceHwInfo->platform.usDeviceID = hwInfo->platform.usDeviceID;
    deviceHwInfo->platform.usRevId = hwInfo->platform.usRevId;
}
void HwHelperTestsXeHpcCore::checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(HardwareInfo *hwInfo) {
    const uint32_t numDevices = 4u;
    const uint32_t tileIndex = 2u;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << tileIndex)};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    DebugManager.flags.EnableLocalMemory.set(true);
    initPlatform();
    auto clDevice = platform()->getClDevice(0);
    setupDeviceIdAndRevision(hwInfo, *clDevice);

    auto commandStreamReceiver = clDevice->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(singleTileMask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

void HwHelperTestsXeHpcCore::checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(HardwareInfo *hwInfo) {
    const uint32_t numDevices = 4u;
    const DeviceBitfield tile0Mask{0x1};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    DebugManager.flags.EnableLocalMemory.set(true);
    DebugManager.flags.OverrideLeastOccupiedBank.set(0u);
    initPlatform();

    auto clDevice = platform()->getClDevice(0);
    setupDeviceIdAndRevision(hwInfo, *clDevice);

    auto commandStreamReceiver = clDevice->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(tile0Mask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}
} // namespace NEO