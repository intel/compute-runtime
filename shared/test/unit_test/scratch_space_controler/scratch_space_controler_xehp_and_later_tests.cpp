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
    void prepareScratchAllocation(uint32_t requiredPerThreadScratchSizeSlot0,
                                  uint32_t requiredPerThreadScratchSizeSlot1,
                                  OsContext &osContext,
                                  bool &stateBaseAddressDirty,
                                  bool &scratchSurfaceDirty,
                                  bool &vfeStateDirty) override {
        scratchSurfaceDirty = scratchDirty;
    }
    ResidencyContainer residencyContainer;
    MockGraphicsAllocation alloc;
    bool wasProgramSurfaceStateAtPtrCalled = false;
    bool scratchDirty = false;
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
