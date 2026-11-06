/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_scratch_space_controller_xehp_and_later.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class MockBindlessScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
  public:
    using ScratchSpaceControllerXeHPAndLater::bindlessSS;
    using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
    using ScratchSpaceControllerXeHPAndLater::singleSurfaceStateSize;

    MockBindlessScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                                   ExecutionEnvironment &environment,
                                                   InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {
        scratchSlot0Allocation = &alloc;
    }
    ~MockBindlessScratchSpaceControllerXeHPAndLater() override {
        scratchSlot0Allocation = nullptr;
    }
    void programSurfaceStateAtPtr(void *surfaceStateForScratchAllocation) override {
        wasProgramSurfaceStateAtPtrCalled = true;
    }
    bool prepareScratchAllocation(uint32_t requiredPerThreadScratchSizeSlot0,
                                  uint32_t requiredPerThreadScratchSizeSlot1,
                                  OsContext &osContext,
                                  bool &scratchSurfaceDirty) override {
        scratchSurfaceDirty = scratchDirty;
        return prepareScratchAllocationResult;
    }
    ResidencyContainer residencyContainer;
    MockGraphicsAllocation alloc;
    bool wasProgramSurfaceStateAtPtrCalled = false;
    bool scratchDirty = false;
    bool prepareScratchAllocationResult = true;
};

using ScratchControllerTests = Test<DeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenDirtyScratchAllocationWhenProgramBindlessHeapThenProgramSurfaceStateAtPtrCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockBindlessScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                                                         *execEnv,
                                                                                                                                                         *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);
    EXPECT_TRUE(scratchController->wasProgramSurfaceStateAtPtrCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenNotDirtyScratchAllocationWhenProgramBindlessHeapThenProgramSurfaceStateAtPtrIsNotCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockBindlessScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                                                         *execEnv,
                                                                                                                                                         *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = false;

    scratchController->bindlessSS = bindlessHeapHelper->allocateSSInHeap(0x1000, nullptr, BindlessHeapsHelper::specialSsh);
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);
    EXPECT_FALSE(scratchController->wasProgramSurfaceStateAtPtrCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenNoBindlessSSWhenProgramBindlessHeapThenMakeResidentIsNotCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockBindlessScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                                                         *execEnv,
                                                                                                                                                         *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = false;
    bindlessHeapHelper->failAllocateSS = true;

    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_EQ(csr.makeResidentCalledTimes, 0u);
    EXPECT_FALSE(scratchController->wasProgramSurfaceStateAtPtrCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenPrivateScratchEnabledWhenProgramBindlessHeapSurfaceThenSSHasDoubleSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockBindlessScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                                                         *execEnv,
                                                                                                                                                         *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    auto usedBefore = bindlessHeapHelper->getHeap(BindlessHeapsHelper::specialSsh)->getUsed();
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    auto usedAfter = bindlessHeapHelper->getHeap(BindlessHeapsHelper::specialSsh)->getUsed();
    EXPECT_EQ(usedAfter - usedBefore, 2 * scratchController->singleSurfaceStateSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenPrivateScratchDisabledWhenProgramBindlessHeapSurfaceThenSSHasSingleSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    debugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockBindlessScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
                                                                                                                                                         *execEnv,
                                                                                                                                                         *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    auto usedBefore = bindlessHeapHelper->getHeap(BindlessHeapsHelper::specialSsh)->getUsed();
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    auto usedAfter = bindlessHeapHelper->getHeap(BindlessHeapsHelper::specialSsh)->getUsed();
    EXPECT_EQ(usedAfter - usedBefore, scratchController->singleSurfaceStateSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenScratchProgrammedInFirstSshWhenScratchGrowsWithSecondSshAndFirstSshIsUsedAgainThenPreviouslyProgrammedSlotInFirstSshIsNotOverwritten) {
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr.getInternalAllocationStorage());
    auto getScratchAddressAtSlot = [&](void *surfaceHeap, uint32_t slot) {
        return reinterpret_cast<const typename FamilyType::DefaultRenderSurfaceState *>(ptrOffset(surfaceHeap, scratchController.getOffsetToSurfaceState(slot)))->getSurfaceBaseAddress();
    };
    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;

    void *firstSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    void *secondSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    scratchController.setRequiredScratchSpace(firstSurfaceHeap, 0u, 0x1000u, 0u, csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty);
    auto firstSlotId = scratchController.slotId;
    auto firstScratchAddress = scratchController.scratchSlot0Allocation->getGpuAddress();

    scratchController.setRequiredScratchSpace(secondSurfaceHeap, 0u, 0x2000u, 0u, csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty);
    auto secondSlotId = scratchController.slotId;
    auto secondScratchAddress = scratchController.scratchSlot0Allocation->getGpuAddress();
    EXPECT_NE(firstSlotId, secondSlotId);
    EXPECT_NE(firstScratchAddress, secondScratchAddress);

    scratchController.setRequiredScratchSpace(firstSurfaceHeap, 0u, 0x2000u, 0u, csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty);
    EXPECT_EQ(secondSlotId, scratchController.slotId);
    EXPECT_EQ(firstScratchAddress, getScratchAddressAtSlot(firstSurfaceHeap, firstSlotId));
    EXPECT_EQ(secondScratchAddress, getScratchAddressAtSlot(firstSurfaceHeap, secondSlotId));

    alignedFree(firstSurfaceHeap);
    alignedFree(secondSurfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenOnlyPrivateScratchAllocatedWhenSshChangesThenScratchIsProgrammedInNewSsh) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePrivateScratchSlot1.set(1);
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr.getInternalAllocationStorage());
    bool stateBaseAddressDirty = false;
    bool cfeStateDirty = false;

    void *firstSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    void *secondSurfaceHeap = alignedMalloc(0x1000, 0x1000);
    memset(secondSurfaceHeap, 0, 0x1000);
    scratchController.setRequiredScratchSpace(firstSurfaceHeap, 0u, 0u, 0x1000u, csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty);
    ASSERT_EQ(nullptr, scratchController.scratchSlot0Allocation);
    ASSERT_NE(nullptr, scratchController.scratchSlot1Allocation);

    cfeStateDirty = false;
    scratchController.setRequiredScratchSpace(secondSurfaceHeap, 0u, 0u, 0x1000u, csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty);
    EXPECT_TRUE(cfeStateDirty);
    auto slot1SurfaceState = ptrOffset(secondSurfaceHeap, scratchController.getOffsetToSurfaceState(scratchController.slotId) + scratchController.singleSurfaceStateSize);
    EXPECT_EQ(scratchController.scratchSlot1Allocation->getGpuAddress(), reinterpret_cast<const typename FamilyType::DefaultRenderSurfaceState *>(slot1SurfaceState)->getSurfaceBaseAddress());

    alignedFree(firstSurfaceHeap);
    alignedFree(secondSurfaceHeap);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenAlternatingSshWhenScratchGrowsThroughAllSupportedSizesThenEveryGrowthGetsNewSlot) {
    DebugManagerStateRestore restorer;
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto maxScratchSize = gfxCoreHelper.getMaxScratchSize(pDevice->getProductHelper());

    for (bool privateScratchSlot1 : {false, true}) {
        debugManager.flags.EnablePrivateScratchSlot1.set(privateScratchSlot1);
        MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->executionEnvironment, *csr.getInternalAllocationStorage());
        scratchController.computeUnitsUsedForScratch = 1u;
        auto heapSize = alignUp(scratchController.getReservedSshSize(), MemoryConstants::pageSize);
        void *heapBuffers[] = {alignedMalloc(heapSize, MemoryConstants::pageSize), alignedMalloc(heapSize, MemoryConstants::pageSize)};
        MockGraphicsAllocation surfaceHeaps[] = {{heapBuffers[0], heapSize}, {heapBuffers[1], heapSize}};
        bool stateBaseAddressDirty = false;
        bool cfeStateDirty = false;
        uint32_t requiredSizes[] = {0u, 0u};
        uint32_t growths = 0u;

        for (uint32_t size = gfxCoreHelper.getMinimalScratchSpaceSize(); size <= maxScratchSize; size *= 2) {
            for (uint32_t scratchSlot = 0u; scratchSlot < (privateScratchSlot1 ? 2u : 1u); scratchSlot++) {
                requiredSizes[scratchSlot] = size;
                HeapContainer heapContainer = {&surfaceHeaps[growths % 2]};
                EXPECT_NO_THROW(scratchController.programHeaps(heapContainer, 1u, requiredSizes[0], requiredSizes[1], csr.getOsContext(), stateBaseAddressDirty, cfeStateDirty));
                growths++;
                EXPECT_EQ(growths, scratchController.slotId);
            }
        }

        alignedFree(heapBuffers[0]);
        alignedFree(heapBuffers[1]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenScratchAllocationFailureWhenSettingRequiredScratchSpaceThenFalseIsReturnedAndPreviousScratchStateIsKept) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    std::vector<uint8_t> surfaceStateHeap(scratchController.getReservedSshSize());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    auto initialScratchAllocation = scratchController.scratchSlot0Allocation;
    ASSERT_NE(nullptr, initialScratchAllocation);
    const auto initialScratchSizeInBytes = scratchController.scratchSlot0SizeInBytes;
    const auto initialPerThreadScratchSize = scratchController.perThreadScratchSpaceSlot0Size;
    const auto initialSlotId = scratchController.slotId;

    memoryManager->forceFailureInPrimaryAllocation = true;
    frontEndStateDirty = false;
    EXPECT_FALSE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x400u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(initialScratchAllocation, scratchController.scratchSlot0Allocation);
    EXPECT_EQ(initialScratchSizeInBytes, scratchController.scratchSlot0SizeInBytes);
    EXPECT_EQ(initialPerThreadScratchSize, scratchController.perThreadScratchSpaceSlot0Size);
    EXPECT_EQ(initialSlotId, scratchController.slotId);

    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x400u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_NE(nullptr, scratchController.scratchSlot0Allocation);
    EXPECT_NE(initialScratchAllocation, scratchController.scratchSlot0Allocation);
    EXPECT_LT(initialScratchSizeInBytes, scratchController.scratchSlot0SizeInBytes);
    EXPECT_EQ(initialSlotId + 1, scratchController.slotId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenSlot1ScratchAllocationFailureWhenSettingRequiredScratchSpaceThenNewSlot0AllocationIsReleasedAndScratchStateIsNotChanged) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    scratchController.twoSlotScratchSpaceSupported = true;
    std::vector<uint8_t> surfaceStateHeap(scratchController.getReservedSshSize());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    uint32_t scratchAllocationsCount = 0u;
    memoryManager->validateAllocateProperties = [&](const AllocationProperties &properties) {
        if (properties.allocationType == AllocationType::scratchSurface) {
            scratchAllocationsCount++;
            memoryManager->forceFailureInPrimaryAllocation = (scratchAllocationsCount == 2u);
        }
    };

    EXPECT_FALSE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x40u, 0x40u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_EQ(2u, scratchAllocationsCount);
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(nullptr, scratchController.scratchSlot0Allocation);
    EXPECT_EQ(nullptr, scratchController.scratchSlot1Allocation);
    EXPECT_EQ(0u, scratchController.scratchSlot0SizeInBytes);
    EXPECT_EQ(0u, scratchController.scratchSlot1SizeInBytes);
    EXPECT_EQ(0u, scratchController.perThreadScratchSpaceSlot0Size);
    EXPECT_EQ(0u, scratchController.perThreadScratchSpaceSlot1Size);

    memoryManager->validateAllocateProperties = [](const AllocationProperties &) -> void {};
    memoryManager->forceFailureInPrimaryAllocation = false;
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenScratchAllocationFailureWhenProgrammingHeapsThenFalseIsReturned) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    std::vector<uint8_t> surfaceStateHeap(scratchController.getReservedSshSize() * 2);
    MockGraphicsAllocation heapAllocation(surfaceStateHeap.data(), surfaceStateHeap.size());
    HeapContainer heapContainer{&heapAllocation};
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_FALSE(scratchController.programHeaps(heapContainer, 1u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(nullptr, scratchController.scratchSlot0Allocation);

    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_TRUE(scratchController.programHeaps(heapContainer, 1u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_NE(nullptr, scratchController.scratchSlot0Allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenReservedScratchSpaceWhenSettingRequiredScratchSpaceThenSurfaceStateIsProgrammedOnlyOnce) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    std::vector<uint8_t> surfaceStateHeap(scratchController.getReservedSshSize());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_TRUE(scratchController.reserveScratchSpace(0x40u, 0u, osContext));
    auto reservedScratchAllocation = scratchController.scratchSlot0Allocation;
    ASSERT_NE(nullptr, reservedScratchAllocation);
    EXPECT_EQ(0u, scratchController.slotId);

    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_EQ(1u, scratchController.slotId);
    EXPECT_EQ(reservedScratchAllocation, scratchController.scratchSlot0Allocation);

    frontEndStateDirty = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(1u, scratchController.slotId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenScratchAllocationFailureWhenReservingScratchSpaceThenFalseIsReturnedAndNothingIsPendingForProgramming) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    std::vector<uint8_t> surfaceStateHeap(scratchController.getReservedSshSize());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_FALSE(scratchController.reserveScratchSpace(0x40u, 0u, osContext));
    EXPECT_EQ(nullptr, scratchController.scratchSlot0Allocation);

    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeap.data(), 0u, 0u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(0u, scratchController.slotId);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenReservedLargerScratchWhenSwitchingBackToPreviouslyUsedHeapThenItsUsedSlotIsNotOverwritten) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;

    MockScratchSpaceControllerXeHPAndLater scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    std::vector<uint8_t> surfaceStateHeapA(scratchController.getReservedSshSize());
    std::vector<uint8_t> surfaceStateHeapB(scratchController.getReservedSshSize());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeapA.data(), 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeapB.data(), 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    const auto usedSlotId = scratchController.slotId;
    const auto usedSlotBegin = scratchController.getOffsetToSurfaceState(usedSlotId);
    const auto usedSlotEnd = scratchController.getOffsetToSurfaceState(usedSlotId + 1);
    const std::vector<uint8_t> usedSlotInHeapA(surfaceStateHeapA.begin() + usedSlotBegin, surfaceStateHeapA.begin() + usedSlotEnd);

    EXPECT_TRUE(scratchController.reserveScratchSpace(0x400u, 0u, osContext));
    frontEndStateDirty = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(surfaceStateHeapA.data(), 0u, 0x400u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));

    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_EQ(usedSlotId + 1, scratchController.slotId);
    EXPECT_EQ(surfaceStateHeapA.data(), reinterpret_cast<uint8_t *>(scratchController.surfaceStateHeap));
    EXPECT_TRUE(std::equal(usedSlotInHeapA.begin(), usedSlotInHeapA.end(), surfaceStateHeapA.begin() + usedSlotBegin));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchControllerTests, givenScratchReservationFailureWhenProgramBindlessHeapThenSurfaceStateIsNotProgrammed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    auto scratchController = std::make_unique<MockBindlessScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice, pDevice->getNumGenericSubDevices() > 1);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    scratchController->prepareScratchAllocationResult = false;

    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0x40u, 0u, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_FALSE(scratchController->wasProgramSurfaceStateAtPtrCalled);
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(nullptr, scratchController->bindlessSS.ssPtr);
}
