/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/surface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_hw_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gmock/gmock.h"

using namespace NEO;

TEST(ClCommandStreamReceiverTest, WhenMakingResidentThenBufferResidencyFlagIsSet) {
    MockContext context;
    auto commandStreamReceiver = context.getDevice(0)->getDefaultEngine().commandStreamReceiver;
    float srcMemory[] = {1.0f};

    auto retVal = CL_INVALID_VALUE;
    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, buffer);

    auto graphicsAllocation = buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    EXPECT_FALSE(graphicsAllocation->isResident(commandStreamReceiver->getOsContext().getContextId()));

    commandStreamReceiver->makeResident(*graphicsAllocation);

    EXPECT_TRUE(graphicsAllocation->isResident(commandStreamReceiver->getOsContext().getContextId()));

    delete buffer;
}

using ClCommandStreamReceiverTests = Test<DeviceFixture>;

HWTEST_F(ClCommandStreamReceiverTests, givenCommandStreamReceiverWhenFenceAllocationIsRequiredAndCreateGlobalFenceAllocationIsCalledThenFenceAllocationIsAllocated) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    MockCsrHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(*pDevice->getDefaultEngine().osContext);
    EXPECT_EQ(nullptr, csr.globalFenceAllocation);

    EXPECT_TRUE(csr.createGlobalFenceAllocation());

    ASSERT_NE(nullptr, csr.globalFenceAllocation);
    EXPECT_EQ(AllocationType::GLOBAL_FENCE, csr.globalFenceAllocation->getAllocationType());
}

HWTEST_F(ClCommandStreamReceiverTests, givenCommandStreamReceiverWhenGettingFenceAllocationThenCorrectFenceAllocationIsReturned) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    CommandStreamReceiverHw<FamilyType> csr(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    csr.setupContext(*pDevice->getDefaultEngine().osContext);
    EXPECT_EQ(nullptr, csr.getGlobalFenceAllocation());

    EXPECT_TRUE(csr.createGlobalFenceAllocation());

    ASSERT_NE(nullptr, csr.getGlobalFenceAllocation());
    EXPECT_EQ(AllocationType::GLOBAL_FENCE, csr.getGlobalFenceAllocation()->getAllocationType());
}

using CommandStreamReceiverMultiRootDeviceTest = MultiRootDeviceFixture;

TEST_F(CommandStreamReceiverMultiRootDeviceTest, WhenCreatingCommandStreamGraphicsAllocationsThenTheyHaveCorrectRootDeviceIndex) {
    auto commandStreamReceiver = &device1->getGpgpuCommandStreamReceiver();

    ASSERT_NE(nullptr, commandStreamReceiver);
    EXPECT_EQ(expectedRootDeviceIndex, commandStreamReceiver->getRootDeviceIndex());

    // Linear stream / Command buffer
    GraphicsAllocation *allocation = mockMemoryManager->allocateGraphicsMemoryWithProperties({expectedRootDeviceIndex, 128u, AllocationType::COMMAND_BUFFER, device1->getDeviceBitfield()});
    LinearStream commandStream{allocation};

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 100u, 0u);
    EXPECT_EQ(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(128u, commandStream.getMaxAvailableSpace());
    EXPECT_EQ(expectedRootDeviceIndex, commandStream.getGraphicsAllocation()->getRootDeviceIndex());

    commandStreamReceiver->ensureCommandBufferAllocation(commandStream, 1024u, 0u);
    EXPECT_NE(allocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(0u, commandStream.getMaxAvailableSpace() % MemoryConstants::pageSize64k);
    EXPECT_EQ(expectedRootDeviceIndex, commandStream.getGraphicsAllocation()->getRootDeviceIndex());
    mockMemoryManager->freeGraphicsMemory(commandStream.getGraphicsAllocation());

    // Debug surface
    auto debugSurface = commandStreamReceiver->allocateDebugSurface(MemoryConstants::pageSize);
    ASSERT_NE(nullptr, debugSurface);
    EXPECT_EQ(expectedRootDeviceIndex, debugSurface->getRootDeviceIndex());

    // Indirect heaps
    IndirectHeap::Type heapTypes[]{IndirectHeap::Type::DYNAMIC_STATE, IndirectHeap::Type::INDIRECT_OBJECT, IndirectHeap::Type::SURFACE_STATE};
    for (auto heapType : heapTypes) {
        IndirectHeap *heap = nullptr;
        commandStreamReceiver->allocateHeapMemory(heapType, MemoryConstants::pageSize, heap);
        ASSERT_NE(nullptr, heap);
        ASSERT_NE(nullptr, heap->getGraphicsAllocation());
        EXPECT_EQ(expectedRootDeviceIndex, heap->getGraphicsAllocation()->getRootDeviceIndex());
        mockMemoryManager->freeGraphicsMemory(heap->getGraphicsAllocation());
        delete heap;
    }

    // Tag allocation
    ASSERT_NE(nullptr, commandStreamReceiver->getTagAllocation());
    EXPECT_EQ(expectedRootDeviceIndex, commandStreamReceiver->getTagAllocation()->getRootDeviceIndex());

    // Preemption allocation
    if (nullptr == commandStreamReceiver->getPreemptionAllocation()) {
        commandStreamReceiver->createPreemptionAllocation();
    }
    EXPECT_EQ(expectedRootDeviceIndex, commandStreamReceiver->getPreemptionAllocation()->getRootDeviceIndex());

    // HostPtr surface
    char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    HostPtrSurface surface(memory, sizeof(memory), true);
    EXPECT_TRUE(commandStreamReceiver->createAllocationForHostSurface(surface, false));
    ASSERT_NE(nullptr, surface.getAllocation());
    EXPECT_EQ(expectedRootDeviceIndex, surface.getAllocation()->getRootDeviceIndex());
}
