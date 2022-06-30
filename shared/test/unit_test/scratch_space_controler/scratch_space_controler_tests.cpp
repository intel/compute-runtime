/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

class MockScratchSpaceControllerBase : public ScratchSpaceControllerBase {
  public:
    MockScratchSpaceControllerBase(uint32_t rootDeviceIndex,
                                   ExecutionEnvironment &environment,
                                   InternalAllocationStorage &allocationStorage) : ScratchSpaceControllerBase(rootDeviceIndex, environment, allocationStorage) {}

    void programHeaps(HeapContainer &heapContainer,
                      uint32_t offset,
                      uint32_t requiredPerThreadScratchSize,
                      uint32_t requiredPerThreadPrivateScratchSize,
                      uint32_t currentTaskCount,
                      OsContext &osContext,
                      bool &stateBaseAddressDirty,
                      bool &vfeStateDirty) override {
        ScratchSpaceControllerBase::programHeaps(heapContainer, offset, requiredPerThreadScratchSize, requiredPerThreadPrivateScratchSize, currentTaskCount, osContext, stateBaseAddressDirty, vfeStateDirty);
        programHeapsCalled = true;
    }
    void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                               uint32_t requiredPerThreadScratchSize,
                                               uint32_t requiredPerThreadPrivateScratchSize,
                                               uint32_t currentTaskCount,
                                               OsContext &osContext,
                                               bool &stateBaseAddressDirty,
                                               bool &vfeStateDirty,
                                               NEO::CommandStreamReceiver *csr) override {
        ScratchSpaceControllerBase::programBindlessSurfaceStateForScratch(heapsHelper, requiredPerThreadScratchSize, requiredPerThreadPrivateScratchSize, currentTaskCount, osContext, stateBaseAddressDirty, vfeStateDirty, csr);
        programBindlessSurfaceStateForScratchCalled = true;
    }
    ResidencyContainer residencyContainer;
    bool programHeapsCalled = false;
    bool programBindlessSurfaceStateForScratchCalled = false;
};

using ScratchComtrolerTests = Test<DeviceFixture>;

HWTEST_F(ScratchComtrolerTests, givenCommandQueueWhenProgramHeapsCalledThenThenProgramHeapsCalled) {
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
    scratchController->programHeaps(heapContainer, 0, 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty);

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerBase *>(scratchController.get())->programHeapsCalled);
}

HWTEST_F(ScratchComtrolerTests, givenCommandQueueWhenProgramHeapBindlessCalledThenThenProgramBindlessSurfaceStateForScratchCalled) {
    MockCsrHw2<FamilyType> csr(*pDevice->getExecutionEnvironment(), 0, pDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*pDevice->getDefaultEngine().osContext);

    ExecutionEnvironment *execEnv = static_cast<ExecutionEnvironment *>(pDevice->getExecutionEnvironment());
    std::unique_ptr<MockScratchSpaceControllerBase> scratchController = std::make_unique<MockScratchSpaceControllerBase>(pDevice->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    HeapContainer heapContainer;
    scratchController->programBindlessSurfaceStateForScratch(nullptr, 0, 0, 0, *pDevice->getDefaultEngine().osContext, gsbaStateDirty, frontEndStateDirty, &csr);

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerBase *>(scratchController.get())->programBindlessSurfaceStateForScratchCalled);
}