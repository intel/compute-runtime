/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

using namespace NEO;

class CommandStreamReceiverHwTestXeHpgCore : public ClDeviceFixture,
                                             public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(1);
        ClDeviceFixture::SetUp();
    }
    void TearDown() override {
        ClDeviceFixture::TearDown();
    }

  private:
    DebugManagerStateRestore restorer;
};

XE_HPG_CORETEST_F(CommandStreamReceiverHwTestXeHpgCore, givenEnableStatelessCompressionWhenCallingGetMemoryCompressionStateThenReturnCorrectValue) {
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
