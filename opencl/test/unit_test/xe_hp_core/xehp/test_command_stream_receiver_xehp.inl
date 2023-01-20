/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

class CommandStreamReceiverHwTestWithLocalMemory : public ClDeviceFixture,
                                                   public HardwareParse,
                                                   public ::testing::Test {
  public:
    void SetUp() override {
        dbgRestore = std::make_unique<DebugManagerStateRestore>();
        DebugManager.flags.EnableLocalMemory.set(1);
        ClDeviceFixture::setUp();
        HardwareParse::setUp();
    }
    void TearDown() override {
        HardwareParse::tearDown();
        ClDeviceFixture::tearDown();
    }

  private:
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenUseClearColorAllocationForBlitterIsNotSetWhenCallingGetClearColorAllocationThenClearAllocationIsNotCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(false);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());

    auto gfxAllocation = commandStreamReceiver.getClearColorAllocation();
    EXPECT_EQ(nullptr, gfxAllocation);
}

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenUseClearColorAllocationForBlitterIsSetWhenCallingGetClearColorAllocationThenClearAllocationIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());

    auto gfxAllocation = commandStreamReceiver.getClearColorAllocation();
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_TRUE(gfxAllocation->storageInfo.readOnlyMultiStorage);
}

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenUseClearColorAllocationForBlitterIsSetWhenClearColorAllocationIsAlreadyCreatedThenCallingGetClearColorAllocationReturnsAlreadyCreatedAllocation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);

    MockCsrHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());

    auto mockAllocation = std::make_unique<MockGraphicsAllocation>();
    auto expectedResult = mockAllocation.get();

    commandStreamReceiver.clearColorAllocation = mockAllocation.release();

    auto gfxAllocation = commandStreamReceiver.getClearColorAllocation();
    EXPECT_EQ(expectedResult, gfxAllocation);
}

template <typename GfxFamily>
struct MockCsrHwWithRace : public MockCsrHw<GfxFamily> {
    MockCsrHwWithRace() = delete;
    MockCsrHwWithRace(ExecutionEnvironment &executionEnvironment,
                      uint32_t rootDeviceIndex,
                      const DeviceBitfield deviceBitfield) : MockCsrHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield) {
        mockGraphicsAllocation.reset(new MockGraphicsAllocation());
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        if (raceLost) {
            this->clearColorAllocation = mockGraphicsAllocation.release();
        }
        return MockCsrHw<GfxFamily>::obtainUniqueOwnership();
    }

    bool raceLost = false;
    std::unique_ptr<MockGraphicsAllocation> mockGraphicsAllocation;
};

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenUseClearColorAllocationForBlitterIsSetWhenCallingGetClearColorAllocationAndRaceIsWonThenClearAllocationIsCreatedInCurrentThread) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);

    MockCsrHwWithRace<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());

    auto gfxAllocation = commandStreamReceiver.getClearColorAllocation();
    EXPECT_EQ(commandStreamReceiver.clearColorAllocation, gfxAllocation);
    EXPECT_NE(commandStreamReceiver.mockGraphicsAllocation.get(), gfxAllocation);
}

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenUseClearColorAllocationForBlitterIsSetWhenCallingGetClearColorAllocationAndRaceIsLostThenClearAllocationIsNotCreatedInCurrentThread) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseClearColorAllocationForBlitter.set(true);

    MockCsrHwWithRace<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    commandStreamReceiver.setupContext(pDevice->getGpgpuCommandStreamReceiver().getOsContext());
    commandStreamReceiver.raceLost = true;

    auto expectedClearColorAllocation = commandStreamReceiver.mockGraphicsAllocation.get();
    auto gfxAllocation = commandStreamReceiver.getClearColorAllocation();
    EXPECT_EQ(commandStreamReceiver.clearColorAllocation, gfxAllocation);
    EXPECT_EQ(expectedClearColorAllocation, gfxAllocation);
}

XEHPTEST_F(CommandStreamReceiverHwTestWithLocalMemory, givenEnableStatelessCompressionWhenCallingGetMemoryCompressionStateThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    CommandStreamReceiverHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    DebugManager.flags.EnableStatelessCompression.set(0);
    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired, pDevice->getHardwareInfo());
        EXPECT_EQ(MemoryCompressionState::NotApplicable, memoryCompressionState);
    }

    DebugManager.flags.EnableStatelessCompression.set(1);
    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired, pDevice->getHardwareInfo());
        if (auxTranslationRequired) {
            EXPECT_EQ(MemoryCompressionState::Disabled, memoryCompressionState);
        } else {
            EXPECT_EQ(MemoryCompressionState::Enabled, memoryCompressionState);
        }
    }
}
