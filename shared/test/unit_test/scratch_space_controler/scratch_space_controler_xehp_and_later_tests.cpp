/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
  public:
    using ScratchSpaceControllerXeHPAndLater::bindlessSS;
    using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
    using ScratchSpaceControllerXeHPAndLater::singleSurfaceStateSize;

    MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                           ExecutionEnvironment &environment,
                                           InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {
        scratchSlot0Allocation = &alloc;
    }
    ~MockScratchSpaceControllerXeHPAndLater() override {
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
    std::unique_ptr<MockScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
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
    std::unique_ptr<MockScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
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
    std::unique_ptr<MockScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
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
    std::unique_ptr<MockScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
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
    std::unique_ptr<MockScratchSpaceControllerXeHPAndLater> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(pDevice->getRootDeviceIndex(),
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
