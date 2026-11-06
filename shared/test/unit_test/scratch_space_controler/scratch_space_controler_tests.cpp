/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class MockScratchSpaceControllerBase : public ScratchSpaceControllerBase {
  public:
    MockScratchSpaceControllerBase(uint32_t rootDeviceIndex,
                                   ExecutionEnvironment &environment,
                                   InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerBase(rootDeviceIndex, environment, allocationStorage) {}

    bool programHeaps(HeapContainer &heapContainer,
                      uint32_t offset,
                      uint32_t requiredPerThreadScratchSizeSlot0,
                      uint32_t requiredPerThreadPrivateScratchSize,
                      OsContext &osContext,
                      bool &stateBaseAddressDirty,
                      bool &vfeStateDirty) override {
        programHeapsCalled = true;
        return ScratchSpaceControllerBase::programHeaps(heapContainer, offset, requiredPerThreadScratchSizeSlot0, requiredPerThreadPrivateScratchSize, osContext, stateBaseAddressDirty, vfeStateDirty);
    }
    void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                               uint32_t requiredPerThreadScratchSizeSlot0,
                                               uint32_t requiredPerThreadPrivateScratchSize,
                                               OsContext &osContext,
                                               bool &stateBaseAddressDirty,
                                               bool &vfeStateDirty,
                                               NEO::CommandStreamReceiver *csr) override {
        ScratchSpaceControllerBase::programBindlessSurfaceStateForScratch(heapsHelper, requiredPerThreadScratchSizeSlot0, requiredPerThreadPrivateScratchSize, osContext, stateBaseAddressDirty, vfeStateDirty, csr);
        programBindlessSurfaceStateForScratchCalled = true;
    }
    ResidencyContainer residencyContainer;
    bool programHeapsCalled = false;
    bool programBindlessSurfaceStateForScratchCalled = false;
};

using ScratchComtrolerTests = Test<DeviceFixture>;

HWTEST_F(ScratchComtrolerTests, givenCommandQueueWhenProgramHeapsCalledThenProgramHeapsCalled) {
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerBase>(pDevice->getRootDeviceIndex(),
                                                                                                                 *execEnv,
                                                                                                                 *csr.getInternalAllocationStorage());

    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    HeapContainer heapContainer;
    EXPECT_TRUE(scratchController->programHeaps(heapContainer, 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty));

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerBase *>(scratchController.get())->programHeapsCalled);
}

HWTEST_F(ScratchComtrolerTests, givenNullptrBindlessHeapHelperWhenProgramBindlessSurfaceStateForScratchCalledThenMakeResidentIsNotCalled) {
    MockCommandStreamReceiver csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerBase> scratchController = std::make_unique<MockScratchSpaceControllerBase>(pDevice->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    HeapContainer heapContainer;
    scratchController->programBindlessSurfaceStateForScratch(nullptr, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerBase *>(scratchController.get())->programBindlessSurfaceStateForScratchCalled);
    EXPECT_EQ(0u, csr.makeResidentCalledTimes);
}

HWTEST_F(ScratchComtrolerTests, givenScratchAllocationFailureWhenSettingRequiredScratchSpaceThenFalseIsReturnedAndScratchStateIsNotChanged) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;
    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->isMockHostMemoryManager = true;
    memoryManager->forceFailureInPrimaryAllocation = true;

    MockScratchSpaceControllerBase scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_FALSE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(stateBaseAddressDirty);
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(nullptr, scratchController.getScratchSpaceSlot0Allocation());
    EXPECT_EQ(0u, scratchController.getPerThreadScratchSpaceSizeSlot0());

    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    auto initialScratchAllocation = scratchController.getScratchSpaceSlot0Allocation();
    EXPECT_NE(nullptr, initialScratchAllocation);
    EXPECT_EQ(0x40u, scratchController.getPerThreadScratchSpaceSizeSlot0());

    memoryManager->forceFailureInPrimaryAllocation = true;
    frontEndStateDirty = false;
    EXPECT_FALSE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x80u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_EQ(initialScratchAllocation, scratchController.getScratchSpaceSlot0Allocation());
    EXPECT_EQ(0x40u, scratchController.getPerThreadScratchSpaceSizeSlot0());

    memoryManager->forceFailureInPrimaryAllocation = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x80u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_NE(initialScratchAllocation, scratchController.getScratchSpaceSlot0Allocation());
    EXPECT_EQ(0x80u, scratchController.getPerThreadScratchSpaceSizeSlot0());
}

HWTEST_F(ScratchComtrolerTests, givenReservedScratchSpaceWhenSettingRequiredScratchSpaceThenFrontEndStateIsMarkedDirtyOnlyOnce) {
    auto &csr = *pDevice->getDefaultEngine().commandStreamReceiver;
    auto &osContext = *pDevice->getDefaultEngine().osContext;

    MockScratchSpaceControllerBase scratchController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *csr.getInternalAllocationStorage());
    bool stateBaseAddressDirty = false;
    bool frontEndStateDirty = false;

    EXPECT_TRUE(scratchController.reserveScratchSpace(0x40u, 0u, osContext));
    auto reservedScratchAllocation = scratchController.getScratchSpaceSlot0Allocation();
    ASSERT_NE(nullptr, reservedScratchAllocation);

    EXPECT_TRUE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_TRUE(frontEndStateDirty);
    EXPECT_EQ(reservedScratchAllocation, scratchController.getScratchSpaceSlot0Allocation());

    frontEndStateDirty = false;
    stateBaseAddressDirty = false;
    EXPECT_TRUE(scratchController.setRequiredScratchSpace(nullptr, 0u, 0x40u, 0u, osContext, stateBaseAddressDirty, frontEndStateDirty));
    EXPECT_FALSE(frontEndStateDirty);
    EXPECT_FALSE(stateBaseAddressDirty);
}
