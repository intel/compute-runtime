/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_plus.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "test.h"

using namespace NEO;

class MockScratchSpaceControllerXeHPPlus : public ScratchSpaceControllerXeHPPlus {
  public:
    using ScratchSpaceControllerXeHPPlus::bindlessSS;
    using ScratchSpaceControllerXeHPPlus::scratchAllocation;
    using ScratchSpaceControllerXeHPPlus::singleSurfaceStateSize;

    MockScratchSpaceControllerXeHPPlus(uint32_t rootDeviceIndex,
                                       ExecutionEnvironment &environment,
                                       InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerXeHPPlus(rootDeviceIndex, environment, allocationStorage) {
        scratchAllocation = &alloc;
    }
    ~MockScratchSpaceControllerXeHPPlus() override {
        scratchAllocation = nullptr;
    }
    void programSurfaceStateAtPtr(void *surfaceStateForScratchAllocation) override {
        wasProgramSurfaceStateAtPtrCalled = true;
    }
    void prepareScratchAllocation(uint32_t requiredPerThreadScratchSize,
                                  uint32_t requiredPerThreadPrivateScratchSize,
                                  uint32_t currentTaskCount,
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

using ScratchComtrolerTests = Test<DeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchComtrolerTests, givenBindlessModeOnWhenGetPatchedOffsetCalledThenBindlessOffsetReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerXeHPPlus> scratchController = std::make_unique<MockScratchSpaceControllerXeHPPlus>(pDevice->getRootDeviceIndex(),
                                                                                                                                 *execEnv,
                                                                                                                                 *csr.getInternalAllocationStorage());
    uint64_t bindlessOffset = 0x4000;
    scratchController->bindlessSS.surfaceStateOffset = bindlessOffset;
    EXPECT_EQ(scratchController->getScratchPatchAddress(), bindlessOffset);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchComtrolerTests, givenDirtyScratchAllocationOnWhenWhenProgramBindlessHeapThenProgramSurfaceStateAtPtrCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerXeHPPlus> scratchController = std::make_unique<MockScratchSpaceControllerXeHPPlus>(pDevice->getRootDeviceIndex(),
                                                                                                                                 *execEnv,
                                                                                                                                 *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);
    EXPECT_TRUE(scratchController->wasProgramSurfaceStateAtPtrCalled);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchComtrolerTests, givenNotDirtyScratchAllocationOnWhenWhenProgramBindlessHeapThenProgramSurfaceStateAtPtrWasNotCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerXeHPPlus> scratchController = std::make_unique<MockScratchSpaceControllerXeHPPlus>(pDevice->getRootDeviceIndex(),
                                                                                                                                 *execEnv,
                                                                                                                                 *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = false;

    scratchController->bindlessSS = bindlessHeapHelper->allocateSSInHeap(0x1000, nullptr, BindlessHeapsHelper::SCRATCH_SSH);
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);
    EXPECT_FALSE(scratchController->wasProgramSurfaceStateAtPtrCalled);
}
HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchComtrolerTests, givenPrivateScratchEnabledWhenWhenProgramBindlessHeapSurfaceThenSSHasDoubleSize) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    DebugManager.flags.EnablePrivateScratchSlot1.set(1);
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerXeHPPlus> scratchController = std::make_unique<MockScratchSpaceControllerXeHPPlus>(pDevice->getRootDeviceIndex(),
                                                                                                                                 *execEnv,
                                                                                                                                 *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    auto usedBefore = bindlessHeapHelper->getHeap(BindlessHeapsHelper::SCRATCH_SSH)->getUsed();
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    auto usedAfter = bindlessHeapHelper->getHeap(BindlessHeapsHelper::SCRATCH_SSH)->getUsed();
    EXPECT_EQ(usedAfter - usedBefore, 2 * scratchController->singleSurfaceStateSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ScratchComtrolerTests, givenPrivateScratchDisabledWhenWhenProgramBindlessHeapSurfaceThenSSHasSingleSize) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    DebugManager.flags.EnablePrivateScratchSlot1.set(0);
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerXeHPPlus> scratchController = std::make_unique<MockScratchSpaceControllerXeHPPlus>(pDevice->getRootDeviceIndex(),
                                                                                                                                 *execEnv,
                                                                                                                                 *csr.getInternalAllocationStorage());
    auto bindlessHeapHelper = std::make_unique<BindlessHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    scratchController->scratchDirty = true;
    auto usedBefore = bindlessHeapHelper->getHeap(BindlessHeapsHelper::SCRATCH_SSH)->getUsed();
    scratchController->programBindlessSurfaceStateForScratch(bindlessHeapHelper.get(), 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);
    auto usedAfter = bindlessHeapHelper->getHeap(BindlessHeapsHelper::SCRATCH_SSH)->getUsed();
    EXPECT_EQ(usedAfter - usedBefore, scratchController->singleSurfaceStateSize);
}
