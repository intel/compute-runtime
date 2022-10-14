/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

constexpr uint32_t defaultNumIddsPerBlock = 64;

using CommandContainerFixture = DeviceFixture;
using CommandContainerTest = Test<CommandContainerFixture>;

class MyMockCommandContainer : public CommandContainer {
  public:
    using CommandContainer::allocationIndirectHeaps;
    using CommandContainer::dirtyHeaps;
    using CommandContainer::getTotalCmdBufferSize;
};

struct CommandContainerHeapStateTests : public ::testing::Test {
    MyMockCommandContainer myCommandContainer;
};

TEST_F(CommandContainerHeapStateTests, givenDirtyHeapsWhenSettingStateForAllThenValuesAreCorrect) {
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), myCommandContainer.dirtyHeaps);
    EXPECT_TRUE(myCommandContainer.isAnyHeapDirty());

    myCommandContainer.setDirtyStateForAllHeaps(false);
    EXPECT_EQ(0u, myCommandContainer.dirtyHeaps);
    EXPECT_FALSE(myCommandContainer.isAnyHeapDirty());

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        HeapType heapType = static_cast<HeapType>(i);
        EXPECT_FALSE(myCommandContainer.isHeapDirty(heapType));
    }

    myCommandContainer.setDirtyStateForAllHeaps(true);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), myCommandContainer.dirtyHeaps);

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        HeapType heapType = static_cast<HeapType>(i);
        EXPECT_TRUE(myCommandContainer.isHeapDirty(heapType));
    }
}

TEST_F(CommandContainerHeapStateTests, givenDirtyHeapsWhenSettingStateForSingleHeapThenValuesAreCorrect) {
    myCommandContainer.dirtyHeaps = 0;
    EXPECT_FALSE(myCommandContainer.isAnyHeapDirty());

    uint32_t controlVariable = 0;
    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        HeapType heapType = static_cast<HeapType>(i);

        EXPECT_FALSE(myCommandContainer.isHeapDirty(heapType));
        myCommandContainer.setHeapDirty(heapType);
        EXPECT_TRUE(myCommandContainer.isHeapDirty(heapType));
        EXPECT_TRUE(myCommandContainer.isAnyHeapDirty());

        controlVariable |= (1 << i);
        EXPECT_EQ(controlVariable, myCommandContainer.dirtyHeaps);
    }

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        HeapType heapType = static_cast<HeapType>(i);
        EXPECT_TRUE(myCommandContainer.isHeapDirty(heapType));
    }
}

TEST_F(CommandContainerTest, givenCmdContainerWhenCreatingCommandBufferThenCorrectAllocationTypeIsSet) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    ASSERT_NE(0u, cmdContainer.getCmdBufferAllocations().size());
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, cmdContainer.getCmdBufferAllocations()[0]->getAllocationType());

    cmdContainer.allocateNextCommandBuffer();

    ASSERT_LE(2u, cmdContainer.getCmdBufferAllocations().size());
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, cmdContainer.getCmdBufferAllocations()[1]->getAllocationType());
}

TEST_F(CommandContainerTest, givenCmdContainerWhenAllocatingHeapsThenSetCorrectAllocationTypes) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        HeapType heapType = static_cast<HeapType>(i);
        auto heap = cmdContainer.getIndirectHeap(heapType);
        if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
            EXPECT_EQ(heap, nullptr);
        } else {
            if (HeapType::INDIRECT_OBJECT == heapType) {
                EXPECT_EQ(AllocationType::INTERNAL_HEAP, heap->getGraphicsAllocation()->getAllocationType());
                EXPECT_NE(0u, heap->getHeapGpuStartOffset());
            } else {
                EXPECT_EQ(AllocationType::LINEAR_STREAM, heap->getGraphicsAllocation()->getAllocationType());
                EXPECT_EQ(0u, heap->getHeapGpuStartOffset());
            }
        }
    }
}

TEST_F(CommandContainerTest, givenCommandContainerWhenInitializeThenEverythingIsInitialized) {
    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(pDevice, nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::SUCCESS, status);

    EXPECT_EQ(pDevice, cmdContainer.getDevice());
    EXPECT_NE(cmdContainer.getHeapHelper(), nullptr);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
    EXPECT_NE(cmdContainer.getCommandStream(), nullptr);

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        auto heapType = static_cast<HeapType>(i);
        auto indirectHeap = cmdContainer.getIndirectHeap(heapType);
        if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
            EXPECT_EQ(indirectHeap, nullptr);
        } else {
            auto heapAllocation = cmdContainer.getIndirectHeapAllocation(heapType);
            EXPECT_EQ(indirectHeap->getGraphicsAllocation(), heapAllocation);
        }
    }

    EXPECT_EQ(cmdContainer.getIddBlock(), nullptr);
    EXPECT_EQ(cmdContainer.getNumIddPerBlock(), defaultNumIddsPerBlock);

    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);

    EXPECT_EQ(cmdContainer.getInstructionHeapBaseAddress(),
              pDevice->getMemoryManager()->getInternalHeapBaseAddress(0, !hwHelper.useSystemMemoryPlacementForISA(pDevice->getHardwareInfo())));
}

TEST_F(CommandContainerTest, givenCommandContainerWhenHeapNotRequiredThenHeapIsNotInitialized) {
    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(pDevice, nullptr, false);
    EXPECT_EQ(CommandContainer::ErrorCode::SUCCESS, status);

    EXPECT_EQ(pDevice, cmdContainer.getDevice());
    EXPECT_EQ(cmdContainer.getHeapHelper(), nullptr);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
    EXPECT_NE(cmdContainer.getCommandStream(), nullptr);

    for (uint32_t i = 0; i < HeapType::NUM_TYPES; i++) {
        auto indirectHeap = cmdContainer.getIndirectHeap(static_cast<HeapType>(i));
        EXPECT_EQ(indirectHeap, nullptr);
    }

    EXPECT_EQ(cmdContainer.getIddBlock(), nullptr);
    EXPECT_EQ(cmdContainer.getNumIddPerBlock(), defaultNumIddsPerBlock);

    EXPECT_EQ(cmdContainer.getInstructionHeapBaseAddress(), 0u);
}

TEST_F(CommandContainerTest, givenEnabledLocalMemoryAndIsaInSystemMemoryWhenCmdContainerIsInitializedThenInstructionBaseAddressIsSetToInternalHeap) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSystemMemoryPlacement.set(1 << (static_cast<uint32_t>(AllocationType::KERNEL_ISA) - 1));

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    const size_t numDevices = 1;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    auto instructionHeapBaseAddress = device->getMemoryManager()->getInternalHeapBaseAddress(0, false);

    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(device.get(), nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::SUCCESS, status);

    EXPECT_EQ(instructionHeapBaseAddress, cmdContainer.getInstructionHeapBaseAddress());
}

TEST_F(CommandContainerTest, givenForceDefaultHeapSizeWhenCmdContainerIsInitializedThenHeapIsCreatedWithProperSize) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceDefaultHeapSize.set(32); // in KB

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    const size_t numDevices = 1;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));

    CommandContainer cmdContainer;
    auto status = cmdContainer.initialize(device.get(), nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::SUCCESS, status);

    auto indirectHeap = cmdContainer.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT);
    EXPECT_EQ(indirectHeap->getAvailableSpace(), 32 * MemoryConstants::kiloByte);
}

TEST_F(CommandContainerTest, givenCommandContainerDuringInitWhenAllocateGfxMemoryFailsThenErrorIsReturned) {
    CommandContainer cmdContainer;
    pDevice->executionEnvironment->memoryManager.reset(new FailMemoryManager(0, *pDevice->executionEnvironment));
    auto status = cmdContainer.initialize(pDevice, nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::OUT_OF_DEVICE_MEMORY, status);
}

TEST_F(CommandContainerTest, givenCmdContainerWithAllocsListWhenAllocateAndResetThenCmdBufferAllocIsReused) {
    AllocationsList allocList;
    auto cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->initialize(pDevice, &allocList, true);
    auto &cmdBufferAllocs = cmdContainer->getCmdBufferAllocations();
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    EXPECT_EQ(memoryManager->handleFenceCompletionCalled, 0u);
    EXPECT_EQ(cmdBufferAllocs.size(), 1u);
    EXPECT_TRUE(allocList.peekIsEmpty());

    cmdContainer->allocateNextCommandBuffer();
    EXPECT_EQ(cmdBufferAllocs.size(), 2u);

    auto cmdBuffer0 = cmdBufferAllocs[0];
    auto cmdBuffer1 = cmdBufferAllocs[1];

    cmdContainer->reset();
    EXPECT_EQ(memoryManager->handleFenceCompletionCalled, 0u);
    EXPECT_EQ(cmdBufferAllocs.size(), 1u);
    EXPECT_EQ(cmdBufferAllocs[0], cmdBuffer0);
    EXPECT_FALSE(allocList.peekIsEmpty());

    cmdContainer->allocateNextCommandBuffer();
    EXPECT_EQ(cmdBufferAllocs.size(), 2u);
    EXPECT_EQ(cmdBufferAllocs[0], cmdBuffer0);
    EXPECT_EQ(cmdBufferAllocs[1], cmdBuffer1);
    EXPECT_TRUE(allocList.peekIsEmpty());

    cmdContainer.reset();
    EXPECT_EQ(memoryManager->handleFenceCompletionCalled, 0u);
    EXPECT_FALSE(allocList.peekIsEmpty());
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenReusableAllocationsAndRemoveUserFenceInCmdlistResetAndDestroyFlagWhenAllocateAndResetThenHandleFenceCompletionIsCalled) {
    DebugManagerStateRestore restore;
    DebugManager.flags.RemoveUserFenceInCmdlistResetAndDestroy.set(0);

    AllocationsList allocList;
    auto cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->initialize(pDevice, &allocList, true);
    auto &cmdBufferAllocs = cmdContainer->getCmdBufferAllocations();
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    EXPECT_EQ(cmdBufferAllocs.size(), 1u);
    cmdContainer->allocateNextCommandBuffer();
    EXPECT_EQ(cmdBufferAllocs.size(), 2u);

    cmdContainer->reset();
    EXPECT_EQ(1u, memoryManager->handleFenceCompletionCalled);
    cmdContainer->allocateNextCommandBuffer();
    EXPECT_EQ(cmdBufferAllocs.size(), 2u);

    cmdContainer.reset();
    EXPECT_EQ(3u, memoryManager->handleFenceCompletionCalled);
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCommandContainerDuringInitWhenAllocateHeapMemoryFailsThenErrorIsReturned) {
    CommandContainer cmdContainer;
    auto tempMemoryManager = pDevice->executionEnvironment->memoryManager.release();
    pDevice->executionEnvironment->memoryManager.reset(new FailMemoryManager(1, *pDevice->executionEnvironment));
    auto status = cmdContainer.initialize(pDevice, nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::OUT_OF_DEVICE_MEMORY, status);
    delete tempMemoryManager;
}

TEST_F(CommandContainerTest, givenCommandContainerWhenSettingIndirectHeapAllocationThenAllocationIsSet) {
    CommandContainer cmdContainer;
    MockGraphicsAllocation mockAllocation;
    auto heapType = HeapType::DYNAMIC_STATE;
    cmdContainer.setIndirectHeapAllocation(heapType, &mockAllocation);
    EXPECT_EQ(cmdContainer.getIndirectHeapAllocation(heapType), &mockAllocation);
}

TEST_F(CommandContainerTest, givenHeapAllocationsWhenDestroyCommandContainerThenHeapAllocationsAreReused) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    auto heapAllocationsAddress = cmdContainer->getIndirectHeapAllocation(HeapType::SURFACE_STATE)->getUnderlyingBuffer();
    cmdContainer.reset(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    bool status = true;
    for (uint32_t i = 0; i < HeapType::NUM_TYPES && !status; i++) {
        auto heapType = static_cast<HeapType>(i);
        if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
            status = status && cmdContainer->getIndirectHeapAllocation(heapType) == nullptr;
        } else {
            status = status && cmdContainer->getIndirectHeapAllocation(heapType)->getUnderlyingBuffer() == heapAllocationsAddress;
        }
    }
    EXPECT_TRUE(status);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenResetThenStateIsReset) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    LinearStream stream;
    uint32_t usedSize = 1;
    cmdContainer.getCommandStream()->getSpace(usedSize);
    EXPECT_EQ(usedSize, cmdContainer.getCommandStream()->getUsed());
    cmdContainer.reset();
    EXPECT_NE(usedSize, cmdContainer.getCommandStream()->getUsed());
    EXPECT_EQ(0u, cmdContainer.getCommandStream()->getUsed());
    EXPECT_EQ(cmdContainer.getIddBlock(), nullptr);
    EXPECT_EQ(cmdContainer.getNumIddPerBlock(), defaultNumIddsPerBlock);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenWantToAddNullPtrToResidencyContainerThenNothingIsAdded) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    auto size = cmdContainer.getResidencyContainer().size();
    cmdContainer.addToResidencyContainer(nullptr);
    EXPECT_EQ(cmdContainer.getResidencyContainer().size(), size);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenWantToAddAlreadyAddedAllocationAndDuplicatesRemovedThenExpectedSizeIsReturned) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    MockGraphicsAllocation mockAllocation;

    auto sizeBefore = cmdContainer.getResidencyContainer().size();

    cmdContainer.addToResidencyContainer(&mockAllocation);
    auto sizeAfterFirstAdd = cmdContainer.getResidencyContainer().size();

    EXPECT_NE(sizeBefore, sizeAfterFirstAdd);

    cmdContainer.addToResidencyContainer(&mockAllocation);
    auto sizeAfterSecondAdd = cmdContainer.getResidencyContainer().size();

    EXPECT_NE(sizeAfterFirstAdd, sizeAfterSecondAdd);

    cmdContainer.removeDuplicatesFromResidencyContainer();
    auto sizeAfterDuplicatesRemoved = cmdContainer.getResidencyContainer().size();

    EXPECT_EQ(sizeAfterFirstAdd, sizeAfterDuplicatesRemoved);
}

HWTEST_F(CommandContainerTest, givenCmdContainerWhenInitializeCalledThenSSHHeapHasBindlessOffsetReserved) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->setReservedSshSize(4 * MemoryConstants::pageSize);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);

    auto heap = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);

    ASSERT_NE(nullptr, heap);
    EXPECT_EQ(4 * MemoryConstants::pageSize, heap->getUsed());
}

HWTEST_F(CommandContainerTest, givenNotEnoughSpaceInSSHWhenGettingHeapWithRequiredSizeAndAlignmentThenSSHHeapHasBindlessOffsetReserved) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->setReservedSshSize(4 * MemoryConstants::pageSize);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);

    auto heap = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);
    ASSERT_NE(nullptr, heap);
    heap->getSpace(heap->getAvailableSpace());

    cmdContainer->getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, sizeof(RENDER_SURFACE_STATE), 0);

    EXPECT_EQ(4 * MemoryConstants::pageSize, heap->getUsed());
    EXPECT_EQ(cmdContainer->sshAllocations.size(), 1u);
}

TEST_F(CommandContainerTest, givenAvailableSpaceWhenGetHeapWithRequiredSizeAndAlignmentCalledThenExistingAllocationIsReturned) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    HeapType heapTypes[] = {HeapType::SURFACE_STATE,
                            HeapType::DYNAMIC_STATE};

    for (auto heapType : heapTypes) {
        auto heapAllocation = cmdContainer->getIndirectHeapAllocation(heapType);
        auto heap = cmdContainer->getIndirectHeap(heapType);

        if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
            EXPECT_EQ(heap, nullptr);
        } else {
            const size_t sizeRequested = 32;
            const size_t alignment = 32;

            EXPECT_GE(heap->getAvailableSpace(), sizeRequested + alignment);
            auto sizeBefore = heap->getUsed();

            auto heapRequested = cmdContainer->getHeapWithRequiredSizeAndAlignment(heapType, sizeRequested, alignment);
            auto newAllocation = heapRequested->getGraphicsAllocation();

            EXPECT_EQ(heap, heapRequested);
            EXPECT_EQ(heapAllocation, newAllocation);

            EXPECT_TRUE((reinterpret_cast<size_t>(heapRequested->getSpace(0)) & (alignment - 1)) == 0);
            EXPECT_FALSE(cmdContainer->isHeapDirty(heapType));

            auto sizeAfter = heapRequested->getUsed();
            EXPECT_EQ(sizeBefore, sizeAfter);
        }
    }
}

TEST_F(CommandContainerTest, givenUnalignedAvailableSpaceWhenGetHeapWithRequiredSizeAndAlignmentCalledThenHeapReturnedIsCorrectlyAligned) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    auto heapAllocation = cmdContainer->getIndirectHeapAllocation(HeapType::SURFACE_STATE);
    auto heap = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);

    const size_t sizeRequested = 32;
    const size_t alignment = 32;

    heap->getSpace(sizeRequested / 2);

    EXPECT_GE(heap->getAvailableSpace(), sizeRequested + alignment);

    auto heapRequested = cmdContainer->getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, sizeRequested, alignment);
    auto newAllocation = heapRequested->getGraphicsAllocation();

    EXPECT_EQ(heap, heapRequested);
    EXPECT_EQ(heapAllocation, newAllocation);

    EXPECT_TRUE((reinterpret_cast<size_t>(heapRequested->getSpace(0)) & (alignment - 1)) == 0);
    EXPECT_FALSE(cmdContainer->isHeapDirty(HeapType::SURFACE_STATE));
}

TEST_F(CommandContainerTest, givenNoAlignmentAndAvailableSpaceWhenGetHeapWithRequiredSizeAndAlignmentCalledThenHeapReturnedIsNotAligned) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    auto heapAllocation = cmdContainer->getIndirectHeapAllocation(HeapType::SURFACE_STATE);
    auto heap = cmdContainer->getIndirectHeap(HeapType::SURFACE_STATE);

    const size_t sizeRequested = 32;
    const size_t alignment = 0;

    heap->getSpace(sizeRequested / 2);

    EXPECT_GE(heap->getAvailableSpace(), sizeRequested + alignment);

    auto heapRequested = cmdContainer->getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, sizeRequested, alignment);
    auto newAllocation = heapRequested->getGraphicsAllocation();

    EXPECT_EQ(heap, heapRequested);
    EXPECT_EQ(heapAllocation, newAllocation);

    EXPECT_TRUE((reinterpret_cast<size_t>(heapRequested->getSpace(0)) & (sizeRequested / 2)) == sizeRequested / 2);
    EXPECT_FALSE(cmdContainer->isHeapDirty(HeapType::SURFACE_STATE));
}

TEST_F(CommandContainerTest, givenNotEnoughSpaceWhenGetHeapWithRequiredSizeAndAlignmentCalledThenNewAllocationIsReturned) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    HeapType heapTypes[] = {HeapType::SURFACE_STATE,
                            HeapType::DYNAMIC_STATE};

    for (auto heapType : heapTypes) {
        auto heapAllocation = cmdContainer->getIndirectHeapAllocation(heapType);
        auto heap = cmdContainer->getIndirectHeap(heapType);

        if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
            EXPECT_EQ(heap, nullptr);
        } else {
            const size_t sizeRequested = 32;
            const size_t alignment = 32;
            size_t availableSize = heap->getAvailableSpace();

            heap->getSpace(availableSize - sizeRequested / 2);

            EXPECT_LT(heap->getAvailableSpace(), sizeRequested + alignment);

            auto heapRequested = cmdContainer->getHeapWithRequiredSizeAndAlignment(heapType, sizeRequested, alignment);
            auto newAllocation = heapRequested->getGraphicsAllocation();

            EXPECT_EQ(heap, heapRequested);
            EXPECT_NE(heapAllocation, newAllocation);

            EXPECT_TRUE((reinterpret_cast<size_t>(heapRequested->getSpace(0)) & (alignment - 1)) == 0);
            EXPECT_TRUE(cmdContainer->isHeapDirty(heapType));
        }
    }
    for (auto deallocation : cmdContainer->getDeallocationContainer()) {
        cmdContainer->getDevice()->getMemoryManager()->freeGraphicsMemory(deallocation);
    }
    cmdContainer->getDeallocationContainer().clear();
}

TEST_F(CommandContainerTest, givenNotEnoughSpaceWhenCreatedAlocationHaveDifferentBaseThenHeapIsDirty) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->setDirtyStateForAllHeaps(false);
    HeapType type = HeapType::INDIRECT_OBJECT;

    auto heapAllocation = cmdContainer->getIndirectHeapAllocation(type);
    auto heap = cmdContainer->getIndirectHeap(type);

    const size_t sizeRequested = 32;
    const size_t alignment = 32;
    size_t availableSize = heap->getAvailableSpace();

    heap->getSpace(availableSize - sizeRequested / 2);

    EXPECT_LT(heap->getAvailableSpace(), sizeRequested + alignment);

    auto heapRequested = cmdContainer->getHeapWithRequiredSizeAndAlignment(type, sizeRequested, alignment);
    auto newAllocation = heapRequested->getGraphicsAllocation();

    EXPECT_EQ(heap, heapRequested);
    EXPECT_NE(heapAllocation, newAllocation);

    EXPECT_TRUE((reinterpret_cast<size_t>(heapRequested->getSpace(0)) & (alignment - 1)) == 0);
    EXPECT_FALSE(cmdContainer->isHeapDirty(type));

    for (auto deallocation : cmdContainer->getDeallocationContainer()) {
        cmdContainer->getDevice()->getMemoryManager()->freeGraphicsMemory(deallocation);
    }
    cmdContainer->getDeallocationContainer().clear();
}

TEST_F(CommandContainerTest, whenAllocateNextCmdBufferIsCalledThenNewAllocationIsCreatedAndCommandStreamReplaced) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);
    auto stream = cmdContainer->getCommandStream();
    ASSERT_NE(nullptr, stream);

    auto initialBuffer = stream->getSpace(0);
    EXPECT_NE(nullptr, initialBuffer);

    cmdContainer->allocateNextCommandBuffer();

    auto nextBuffer = stream->getSpace(0);
    auto sizeUsed = stream->getUsed();
    auto availableSize = stream->getMaxAvailableSpace();

    EXPECT_NE(nullptr, nextBuffer);
    EXPECT_EQ(0u, sizeUsed);
    EXPECT_NE(initialBuffer, nextBuffer);
    size_t alignedSize = alignUp<size_t>(CommandContainer::totalCmdBufferSize, MemoryConstants::pageSize64k);
    const size_t cmdBufSize = alignedSize - CommandContainer::cmdBufferReservedSize;
    EXPECT_EQ(cmdBufSize, availableSize);

    ASSERT_EQ(2u, cmdContainer->getCmdBufferAllocations().size());
    EXPECT_EQ(cmdContainer->getCmdBufferAllocations()[1], cmdContainer->getCommandStream()->getGraphicsAllocation());

    EXPECT_EQ(cmdContainer->getCmdBufferAllocations()[1], cmdContainer->getResidencyContainer().back());
}

TEST_F(CommandContainerTest, whenResettingCommandContainerThenStoredCmdBuffersAreFreedAndStreamIsReplacedWithInitialBuffer) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer);
    cmdContainer->initialize(pDevice, nullptr, true);

    cmdContainer->allocateNextCommandBuffer();
    cmdContainer->allocateNextCommandBuffer();

    EXPECT_EQ(3u, cmdContainer->getCmdBufferAllocations().size());

    cmdContainer->reset();

    ASSERT_EQ(1u, cmdContainer->getCmdBufferAllocations().size());

    auto stream = cmdContainer->getCommandStream();
    ASSERT_NE(nullptr, stream);

    auto buffer = stream->getSpace(0);
    const size_t cmdBufSize = CommandContainer::defaultListCmdBufferSize;

    EXPECT_EQ(cmdContainer->getCmdBufferAllocations()[0]->getUnderlyingBuffer(), buffer);
    EXPECT_EQ(cmdBufSize, stream->getMaxAvailableSpace());
}

class CommandContainerHeaps : public DeviceFixture,
                              public ::testing::TestWithParam<IndirectHeap::Type> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

INSTANTIATE_TEST_CASE_P(
    Device,
    CommandContainerHeaps,
    testing::Values(
        IndirectHeap::Type::DYNAMIC_STATE,
        IndirectHeap::Type::INDIRECT_OBJECT,
        IndirectHeap::Type::SURFACE_STATE));

TEST_P(CommandContainerHeaps, givenCommandContainerWhenGetAllowHeapGrowCalledThenHeapIsReturned) {
    HeapType heapType = GetParam();

    CommandContainer cmdContainer;

    cmdContainer.initialize(pDevice, nullptr, true);
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
        EXPECT_EQ(cmdContainer.getIndirectHeap(heapType), nullptr);
    } else {
        auto usedSpaceBefore = cmdContainer.getIndirectHeap(heapType)->getUsed();
        size_t size = 5000;
        void *ptr = cmdContainer.getHeapSpaceAllowGrow(heapType, size);
        ASSERT_NE(nullptr, ptr);

        auto usedSpaceAfter = cmdContainer.getIndirectHeap(heapType)->getUsed();
        ASSERT_EQ(usedSpaceBefore + size, usedSpaceAfter);
    }
}

TEST_P(CommandContainerHeaps, givenCommandContainerWhenGetingMoreThanAvailableSizeThenBiggerHeapIsReturned) {
    HeapType heapType = GetParam();

    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    cmdContainer.setDirtyStateForAllHeaps(false);
    auto heap = cmdContainer.getIndirectHeap(heapType);
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
        EXPECT_EQ(heap, nullptr);
    } else {
        auto usedSpaceBefore = heap->getUsed();
        auto availableSizeBefore = heap->getAvailableSpace();

        void *ptr = cmdContainer.getHeapSpaceAllowGrow(heapType, availableSizeBefore + 1);
        ASSERT_NE(nullptr, ptr);

        auto usedSpaceAfter = heap->getUsed();
        auto availableSizeAfter = heap->getAvailableSpace();
        EXPECT_GT(usedSpaceAfter + availableSizeAfter, usedSpaceBefore + availableSizeBefore);
        EXPECT_EQ(!cmdContainer.isHeapDirty(heapType), heapType == IndirectHeap::Type::INDIRECT_OBJECT);
    }
}

TEST_P(CommandContainerHeaps, givenCommandContainerForDifferentRootDevicesThenHeapsAreCreatedWithCorrectRootDeviceIndex) {
    HeapType heapType = GetParam();

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    const size_t numDevices = 2;

    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->calculateMaxOsContextCount();
    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    CommandContainer cmdContainer0;
    cmdContainer0.initialize(device0.get(), nullptr, true);

    CommandContainer cmdContainer1;
    cmdContainer1.initialize(device1.get(), nullptr, true);
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages && HeapType::DYNAMIC_STATE == heapType) {
        EXPECT_EQ(cmdContainer0.getIndirectHeap(heapType), nullptr);
        EXPECT_EQ(cmdContainer1.getIndirectHeap(heapType), nullptr);
    } else {
        uint32_t heapRootDeviceIndex0 = cmdContainer0.getIndirectHeap(heapType)->getGraphicsAllocation()->getRootDeviceIndex();
        EXPECT_EQ(device0->getRootDeviceIndex(), heapRootDeviceIndex0);

        uint32_t heapRootDeviceIndex1 = cmdContainer1.getIndirectHeap(heapType)->getGraphicsAllocation()->getRootDeviceIndex();
        EXPECT_EQ(device1->getRootDeviceIndex(), heapRootDeviceIndex1);
    }
}

TEST_F(CommandContainerHeaps, givenCommandContainerForDifferentRootDevicesThenCmdBufferAllocationIsCreatedWithCorrectRootDeviceIndex) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    const size_t numDevices = 2;

    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->calculateMaxOsContextCount();
    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    CommandContainer cmdContainer0;
    cmdContainer0.initialize(device0.get(), nullptr, true);
    EXPECT_EQ(1u, cmdContainer0.getCmdBufferAllocations().size());
    uint32_t cmdBufferAllocationIndex0 = cmdContainer0.getCmdBufferAllocations().front()->getRootDeviceIndex();
    EXPECT_EQ(device0->getRootDeviceIndex(), cmdBufferAllocationIndex0);

    CommandContainer cmdContainer1;
    cmdContainer1.initialize(device1.get(), nullptr, true);
    EXPECT_EQ(1u, cmdContainer1.getCmdBufferAllocations().size());
    uint32_t cmdBufferAllocationIndex1 = cmdContainer1.getCmdBufferAllocations().front()->getRootDeviceIndex();
    EXPECT_EQ(device1->getRootDeviceIndex(), cmdBufferAllocationIndex1);
}

TEST_F(CommandContainerHeaps, givenCommandContainerForDifferentRootDevicesThenInternalHeapIsCreatedWithCorrectRootDeviceIndex) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    const size_t numDevices = 2;
    executionEnvironment->prepareRootDeviceEnvironments(numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    executionEnvironment->calculateMaxOsContextCount();
    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    auto &hwHelper0 = HwHelper::get(device0->getHardwareInfo().platform.eRenderCoreFamily);
    auto &hwHelper1 = HwHelper::get(device1->getHardwareInfo().platform.eRenderCoreFamily);

    CommandContainer cmdContainer0;
    cmdContainer0.initialize(device0.get(), nullptr, true);
    bool useLocalMemory0 = !hwHelper0.useSystemMemoryPlacementForISA(device0->getHardwareInfo());
    uint64_t baseAddressHeapDevice0 = device0->getMemoryManager()->getInternalHeapBaseAddress(device0->getRootDeviceIndex(), useLocalMemory0);
    EXPECT_EQ(cmdContainer0.getInstructionHeapBaseAddress(), baseAddressHeapDevice0);

    CommandContainer cmdContainer1;
    cmdContainer1.initialize(device1.get(), nullptr, true);
    bool useLocalMemory1 = !hwHelper1.useSystemMemoryPlacementForISA(device0->getHardwareInfo());
    uint64_t baseAddressHeapDevice1 = device1->getMemoryManager()->getInternalHeapBaseAddress(device1->getRootDeviceIndex(), useLocalMemory1);
    EXPECT_EQ(cmdContainer1.getInstructionHeapBaseAddress(), baseAddressHeapDevice1);
}

TEST_F(CommandContainerTest, givenCommandContainerWhenDestructionThenNonHeapAllocationAreNotDestroyed) {
    std::unique_ptr<CommandContainer> cmdContainer(new CommandContainer());
    MockGraphicsAllocation alloc;
    size_t size = 0x1000;
    alloc.setSize(size);
    cmdContainer->initialize(pDevice, nullptr, true);
    cmdContainer->getDeallocationContainer().push_back(&alloc);
    cmdContainer.reset();
    EXPECT_EQ(alloc.getUnderlyingBufferSize(), size);
}

TEST_F(CommandContainerTest, givenContainerAllocatesNextCommandBufferWhenResetingContainerThenExpectFirstCommandBufferAllocationIsReused) {
    auto cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->initialize(pDevice, nullptr, true);

    auto stream = cmdContainer->getCommandStream();
    ASSERT_NE(nullptr, stream);
    auto firstCmdBufferAllocation = stream->getGraphicsAllocation();
    ASSERT_NE(nullptr, firstCmdBufferAllocation);
    auto firstCmdBufferCpuPointer = stream->getSpace(0);
    EXPECT_EQ(firstCmdBufferCpuPointer, firstCmdBufferAllocation->getUnderlyingBuffer());

    cmdContainer->allocateNextCommandBuffer();
    auto secondCmdBufferAllocation = stream->getGraphicsAllocation();
    ASSERT_NE(nullptr, secondCmdBufferAllocation);
    EXPECT_NE(firstCmdBufferAllocation, secondCmdBufferAllocation);
    auto secondCmdBufferCpuPointer = stream->getSpace(0);
    EXPECT_EQ(secondCmdBufferCpuPointer, secondCmdBufferAllocation->getUnderlyingBuffer());
    EXPECT_NE(firstCmdBufferCpuPointer, secondCmdBufferCpuPointer);

    cmdContainer->reset();

    auto aferResetCmdBufferAllocation = stream->getGraphicsAllocation();
    ASSERT_NE(nullptr, aferResetCmdBufferAllocation);
    auto afterResetCmdBufferCpuPointer = stream->getSpace(0);
    EXPECT_EQ(afterResetCmdBufferCpuPointer, aferResetCmdBufferAllocation->getUnderlyingBuffer());

    EXPECT_EQ(firstCmdBufferAllocation, aferResetCmdBufferAllocation);
    EXPECT_EQ(firstCmdBufferCpuPointer, afterResetCmdBufferCpuPointer);

    bool firstAllocationFound = false;
    auto &residencyContainer = cmdContainer->getResidencyContainer();
    for (auto *allocation : residencyContainer) {
        if (allocation == firstCmdBufferAllocation) {
            firstAllocationFound = true;
            break;
        }
    }
    EXPECT_TRUE(firstAllocationFound);
}

class MyLinearStreamMock : public LinearStream {
  public:
    using LinearStream::cmdContainer;
};

TEST_F(CommandContainerTest, givenCmdContainerWhenContainerIsInitializedThenStreamContainsContainerPtr) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    EXPECT_EQ(reinterpret_cast<MyLinearStreamMock *>(cmdContainer.getCommandStream())->cmdContainer, &cmdContainer);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenContainerIsInitializedThenStreamSizeEqualAlignedTotalCmdBuffSizeDecreasedOfReservedSize) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    size_t alignedSize = alignUp<size_t>(CommandContainer::totalCmdBufferSize, MemoryConstants::pageSize64k);
    EXPECT_EQ(cmdContainer.getCommandStream()->getMaxAvailableSpace(), alignedSize - CommandContainer::cmdBufferReservedSize);
}

TEST_F(CommandContainerTest, GivenCmdContainerAndDebugFlagWhenContainerIsInitializedThenStreamSizeEqualsAlignedTotalCmdBuffSizeDecreasedOfReservedSize) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.set(0);
    MyMockCommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    size_t alignedSize = alignUp<size_t>(cmdContainer.getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    EXPECT_EQ(cmdContainer.getCommandStream()->getMaxAvailableSpace(), alignedSize - MyMockCommandContainer::cmdBufferReservedSize);

    auto newSizeInKB = 512;
    DebugManager.flags.OverrideCmdListCmdBufferSizeInKb.set(newSizeInKB);
    MyMockCommandContainer cmdContainer2;
    cmdContainer2.initialize(pDevice, nullptr, true);
    alignedSize = alignUp<size_t>(cmdContainer.getTotalCmdBufferSize(), MemoryConstants::pageSize64k);
    EXPECT_EQ(cmdContainer2.getCommandStream()->getMaxAvailableSpace(), alignedSize - MyMockCommandContainer::cmdBufferReservedSize);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenAlocatingNextCmdBufferThenStreamSizeEqualAlignedTotalCmdBuffSizeDecreasedOfReservedSize) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    cmdContainer.allocateNextCommandBuffer();
    size_t alignedSize = alignUp<size_t>(CommandContainer::totalCmdBufferSize, MemoryConstants::pageSize64k);
    EXPECT_EQ(cmdContainer.getCommandStream()->getMaxAvailableSpace(), alignedSize - CommandContainer::cmdBufferReservedSize);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenCloseAndAllocateNextCommandBufferCalledThenBBEndPlacedAtEndOfLinearStream) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto ptr = cmdContainer.getCommandStream()->getSpace(0u);
    cmdContainer.closeAndAllocateNextCommandBuffer();
    EXPECT_EQ(memcmp(ptr, hwHelper.getBatchBufferEndReference(), hwHelper.getBatchBufferEndSize()), 0);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenCloseAndAllocateNextCommandBufferCalledThenNewCmdBufferAllocationCreated) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 1u);
    cmdContainer.closeAndAllocateNextCommandBuffer();
    EXPECT_EQ(cmdContainer.getCmdBufferAllocations().size(), 2u);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenSetCmdBufferThenCmdBufferSetCorrectly) {
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);

    AllocationProperties properties{pDevice->getRootDeviceIndex(),
                                    true /* allocateMemory*/,
                                    2048,
                                    AllocationType::COMMAND_BUFFER,
                                    (pDevice->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                    false,
                                    pDevice->getDeviceBitfield()};

    auto alloc = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    cmdContainer.setCmdBuffer(alloc);
    EXPECT_EQ(cmdContainer.getCommandStream()->getGraphicsAllocation(), alloc);
    pDevice->getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenReuseExistingCmdBufferWithoutAnyAllocationInListThenReturnNullptr) {
    auto cmdContainer = std::make_unique<CommandContainer>();
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;
    cmdContainer->setImmediateCmdListCsr(csr);

    EXPECT_EQ(cmdContainer->reuseExistingCmdBuffer(), nullptr);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

HWTEST_F(CommandContainerTest, givenCmdContainerWhenReuseExistingCmdBufferWithAllocationInListAndCsrTaskCountLowerThanAllocationThenReturnNullptr) {
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    *csr.tagAddress = 0u;

    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);
    cmdContainer->setImmediateCmdListCsr(&csr);
    cmdContainer->getCmdBufferAllocations()[0]->updateTaskCount(10, 0);
    auto currectContainerSize = cmdContainer->getCmdBufferAllocations().size();
    cmdContainer->addCurrentCommandBufferToReusableAllocationList();
    EXPECT_EQ(cmdContainer->getCmdBufferAllocations().size(), currectContainerSize - 1);

    EXPECT_EQ(cmdContainer->reuseExistingCmdBuffer(), nullptr);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

HWTEST_F(CommandContainerTest, givenCmdContainerWhenReuseExistingCmdBufferWithAllocationInListAndCsrTaskCountSameAsAllocationThenReturnAlloc) {
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    *csr.tagAddress = 10u;

    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);
    cmdContainer->setImmediateCmdListCsr(&csr);

    cmdContainer->getCmdBufferAllocations()[0]->updateTaskCount(10, 0);
    cmdContainer->addCurrentCommandBufferToReusableAllocationList();

    auto currectContainerSize = cmdContainer->getCmdBufferAllocations().size();
    EXPECT_NE(cmdContainer->reuseExistingCmdBuffer(), nullptr);
    EXPECT_EQ(cmdContainer->getCmdBufferAllocations().size(), currectContainerSize + 1);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, GivenCmdContainerWhenContainerIsInitializedThenSurfaceStateIndirectHeapSizeIsCorrect) {
    MyMockCommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, true);
    auto size = cmdContainer.allocationIndirectHeaps[IndirectHeap::Type::SURFACE_STATE]->getUnderlyingBufferSize();
    constexpr size_t expectedHeapSize = MemoryConstants::pageSize64k;
    EXPECT_EQ(expectedHeapSize, size);
}

HWTEST_F(CommandContainerTest, givenCmdContainerHasImmediateCsrWhenGettingHeapWithoutEnsuringSpaceThenExpectNullptrReturnedOrUnrecoverable) {
    CommandContainer cmdContainer;
    cmdContainer.enableHeapSharing();
    cmdContainer.setImmediateCmdListCsr(pDevice->getDefaultEngine().commandStreamReceiver);
    cmdContainer.setNumIddPerBlock(1);
    auto code = cmdContainer.initialize(pDevice, nullptr, true);
    EXPECT_EQ(CommandContainer::ErrorCode::SUCCESS, code);

    EXPECT_EQ(nullptr, cmdContainer.getIndirectHeap(HeapType::DYNAMIC_STATE));
    EXPECT_EQ(nullptr, cmdContainer.getIndirectHeap(HeapType::SURFACE_STATE));

    EXPECT_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, 0), std::exception);
    EXPECT_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::DYNAMIC_STATE, 0, 0), std::exception);

    EXPECT_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::SURFACE_STATE, 0), std::exception);
    EXPECT_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, 0, 0), std::exception);

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.recursiveLockCounter = 0;

    cmdContainer.ensureHeapSizePrepared(0, 0);
    EXPECT_EQ(1u, ultCsr.recursiveLockCounter);

    EXPECT_EQ(nullptr, cmdContainer.getIndirectHeap(HeapType::DYNAMIC_STATE));
    EXPECT_NE(nullptr, cmdContainer.getIndirectHeap(HeapType::SURFACE_STATE));

    EXPECT_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, 0), std::exception);
    EXPECT_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::DYNAMIC_STATE, 0, 0), std::exception);

    EXPECT_NO_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::SURFACE_STATE, 0));
    EXPECT_NO_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, 0, 0));

    cmdContainer.ensureHeapSizePrepared(4 * MemoryConstants::kiloByte, 4 * MemoryConstants::kiloByte);
    EXPECT_EQ(2u, ultCsr.recursiveLockCounter);

    auto dshHeap = cmdContainer.getIndirectHeap(HeapType::DYNAMIC_STATE);
    EXPECT_NE(nullptr, dshHeap);
    auto sshHeap = cmdContainer.getIndirectHeap(HeapType::SURFACE_STATE);
    EXPECT_NE(nullptr, sshHeap);

    size_t sizeUsedDsh = dshHeap->getUsed();
    size_t sizeUsedSsh = sshHeap->getUsed();

    void *dshPtr = cmdContainer.getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, 64);
    void *sshPtr = cmdContainer.getHeapSpaceAllowGrow(HeapType::SURFACE_STATE, 64);

    EXPECT_EQ(ptrOffset(dshHeap->getCpuBase(), sizeUsedDsh), dshPtr);
    EXPECT_EQ(ptrOffset(sshHeap->getCpuBase(), sizeUsedSsh), sshPtr);

    auto alignedHeapDsh = cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::DYNAMIC_STATE, 128, 128);
    auto alignedHeapSsh = cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, 128, 128);

    EXPECT_EQ(dshHeap, alignedHeapDsh);
    EXPECT_EQ(sshHeap, alignedHeapSsh);

    dshHeap->getSpace(dshHeap->getAvailableSpace() - 32);
    sshHeap->getSpace(sshHeap->getAvailableSpace() - 32);

    EXPECT_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::DYNAMIC_STATE, 64), std::exception);
    EXPECT_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::DYNAMIC_STATE, 64, 64), std::exception);

    EXPECT_THROW(cmdContainer.getHeapSpaceAllowGrow(HeapType::SURFACE_STATE, 64), std::exception);
    EXPECT_THROW(cmdContainer.getHeapWithRequiredSizeAndAlignment(HeapType::SURFACE_STATE, 64, 64), std::exception);
}

struct MockHeapHelper : public HeapHelper {
  public:
    using HeapHelper::storageForReuse;
};

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsThenAllocListsNotEmptyAndMadeResident) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;

    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, true);
    cmdContainer->setImmediateCmdListCsr(csr);
    auto heapHelper = reinterpret_cast<MockHeapHelper *>(cmdContainer->getHeapHelper());

    EXPECT_TRUE(allocList.peekIsEmpty());
    EXPECT_TRUE(heapHelper->storageForReuse->getAllocationsForReuse().peekIsEmpty());
    auto actualResidencyContainerSize = cmdContainer->getResidencyContainer().size();
    cmdContainer->fillReusableAllocationLists();
    EXPECT_FALSE(allocList.peekIsEmpty());
    EXPECT_FALSE(heapHelper->storageForReuse->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(heapHelper->storageForReuse->getAllocationsForReuse().peekHead()->getResidencyTaskCount(csr->getOsContext().getContextId()), 1u);
    EXPECT_EQ(cmdContainer->getResidencyContainer().size(), actualResidencyContainerSize + 1);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsWithSharedHeapsEnabledThenOnlyOneHeapFilled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;

    AllocationsList allocList;
    cmdContainer->enableHeapSharing();
    cmdContainer->initialize(pDevice, &allocList, true);
    cmdContainer->setImmediateCmdListCsr(csr);

    auto &reusableHeapsList = reinterpret_cast<MockHeapHelper *>(cmdContainer->getHeapHelper())->storageForReuse->getAllocationsForReuse();

    EXPECT_TRUE(reusableHeapsList.peekIsEmpty());
    cmdContainer->fillReusableAllocationLists();
    EXPECT_FALSE(reusableHeapsList.peekIsEmpty());
    EXPECT_EQ(reusableHeapsList.peekHead()->countThisAndAllConnected(), 1u);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsWithBindlessModeEnabledThenOnlyOneHeapFilled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;
    auto cmdContainer = std::make_unique<CommandContainer>();
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, true);
    cmdContainer->setImmediateCmdListCsr(csr);

    auto &reusableHeapsList = reinterpret_cast<MockHeapHelper *>(cmdContainer->getHeapHelper())->storageForReuse->getAllocationsForReuse();

    EXPECT_TRUE(reusableHeapsList.peekIsEmpty());

    DebugManager.flags.UseBindlessMode.set(true);
    cmdContainer->fillReusableAllocationLists();
    EXPECT_FALSE(reusableHeapsList.peekIsEmpty());
    EXPECT_EQ(reusableHeapsList.peekHead()->countThisAndAllConnected(), 1u);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsWithoutHeapsThenAllocListNotEmpty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    auto cmdContainer = std::make_unique<CommandContainer>();
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);

    EXPECT_TRUE(allocList.peekIsEmpty());
    cmdContainer->fillReusableAllocationLists();
    EXPECT_FALSE(allocList.peekIsEmpty());

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsWithSpecifiedAmountThenAllocationsCreated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(10);
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);
    cmdContainer->setImmediateCmdListCsr(csr);

    EXPECT_TRUE(allocList.peekIsEmpty());
    cmdContainer->fillReusableAllocationLists();
    EXPECT_EQ(allocList.peekHead()->countThisAndAllConnected(), 10u);

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerAndCsrWhenGetHeapWithRequiredSizeAndAlignmentThenReuseAllocationIfAvailable) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, true);
    cmdContainer->setImmediateCmdListCsr(csr);

    cmdContainer->fillReusableAllocationLists();
    auto &reusableHeapsList = reinterpret_cast<MockHeapHelper *>(cmdContainer->getHeapHelper())->storageForReuse->getAllocationsForReuse();
    auto baseAlloc = cmdContainer->getIndirectHeapAllocation(HeapType::INDIRECT_OBJECT);
    auto reusableAlloc = reusableHeapsList.peekHead();

    cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT)->getSpace(cmdContainer->getIndirectHeap(HeapType::INDIRECT_OBJECT)->getMaxAvailableSpace());
    auto heap = cmdContainer->getHeapWithRequiredSizeAndAlignment(HeapType::INDIRECT_OBJECT, 1024, 1024);

    EXPECT_EQ(heap->getGraphicsAllocation(), reusableAlloc);
    EXPECT_TRUE(reusableHeapsList.peekContains(*baseAlloc));

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}

TEST_F(CommandContainerTest, givenCmdContainerWhenFillReusableAllocationListsAndFlagDisabledThenAllocListEmpty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);
    auto cmdContainer = std::make_unique<CommandContainer>();
    auto csr = pDevice->getDefaultEngine().commandStreamReceiver;
    AllocationsList allocList;
    cmdContainer->initialize(pDevice, &allocList, false);
    cmdContainer->setImmediateCmdListCsr(csr);

    EXPECT_TRUE(allocList.peekIsEmpty());
    cmdContainer->fillReusableAllocationLists();
    EXPECT_TRUE(allocList.peekIsEmpty());

    cmdContainer.reset();
    allocList.freeAllGraphicsAllocations(pDevice);
}