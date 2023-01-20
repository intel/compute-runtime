/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

#include "hw_cmds_xe_hpg_core_base.h"

using namespace NEO;

class CommandStreamReceiverHwTestXeHpgCore : public ClDeviceFixture,
                                             public ::testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(1);
        ClDeviceFixture::setUp();
    }
    void TearDown() override {
        ClDeviceFixture::tearDown();
    }

  private:
    DebugManagerStateRestore restorer;
};

XE_HPG_CORETEST_F(CommandStreamReceiverHwTestXeHpgCore, givenEnableStatelessCompressionWhenCallingGetMemoryCompressionStateThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    CommandStreamReceiverHw<FamilyType> commandStreamReceiver(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    DebugManager.flags.EnableStatelessCompression.set(0);
    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired);
        EXPECT_EQ(MemoryCompressionState::NotApplicable, memoryCompressionState);
    }

    DebugManager.flags.EnableStatelessCompression.set(1);
    for (bool auxTranslationRequired : {false, true}) {
        auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(auxTranslationRequired);
        if (auxTranslationRequired) {
            EXPECT_EQ(MemoryCompressionState::Disabled, memoryCompressionState);
        } else {
            EXPECT_EQ(MemoryCompressionState::Enabled, memoryCompressionState);
        }
    }
}
