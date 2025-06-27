/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/indirect_heap/indirect_heap.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {

void ClGfxCoreHelperXeHpcCoreFixture::setupDeviceIdAndRevision(HardwareInfo *hwInfo, ClDevice &clDevice) {
    auto deviceHwInfo = clDevice.getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    deviceHwInfo->platform.usDeviceID = hwInfo->platform.usDeviceID;
    deviceHwInfo->platform.usRevId = hwInfo->platform.usRevId;
}
void ClGfxCoreHelperXeHpcCoreFixture::checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(HardwareInfo *hwInfo) {
    const uint32_t numDevices = 4u;
    const uint32_t tileIndex = 2u;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << tileIndex)};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    debugManager.flags.EnableLocalMemory.set(true);
    initPlatform();
    auto clDevice = platform()->getClDevice(0);
    setupDeviceIdAndRevision(hwInfo, *clDevice);

    auto commandStreamReceiver = clDevice->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::indirectObject, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps()) {
        EXPECT_EQ(AllocationType::internalHeap, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::linearStream, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(singleTileMask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::commandBuffer, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::localMemory);
}

void ClGfxCoreHelperXeHpcCoreFixture::checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(HardwareInfo *hwInfo) {
    const uint32_t numDevices = 4u;
    const DeviceBitfield tile0Mask{0x1};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    debugManager.flags.CreateMultipleSubDevices.set(numDevices);
    debugManager.flags.EnableLocalMemory.set(true);
    debugManager.flags.OverrideLeastOccupiedBank.set(0u);
    initPlatform();

    auto clDevice = platform()->getClDevice(0);
    setupDeviceIdAndRevision(hwInfo, *clDevice);

    auto commandStreamReceiver = clDevice->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::indirectObject, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps()) {
        EXPECT_EQ(AllocationType::internalHeap, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::linearStream, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(tile0Mask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::commandBuffer, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::localMemory);
}
} // namespace NEO
