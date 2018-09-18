/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

using namespace OCLRT;

struct CreateCommandStreamReceiverTest : public GmmEnvironmentFixture, public ::testing::TestWithParam<CommandStreamReceiverType> {
    void SetUp() override {
        GmmEnvironmentFixture::SetUp();
        storeInitHWTag = initialHardwareTag;
    }

    void TearDown() override {
        initialHardwareTag = storeInitHWTag;
        GmmEnvironmentFixture::TearDown();
    }

  protected:
    int storeInitHWTag;
};

HWTEST_P(CreateCommandStreamReceiverTest, givenCreateCommandStreamWhenCsrIsSetToValidTypeThenTheFuntionReturnsCommandStreamReceiver) {
    DebugManagerStateRestore stateRestorer;
    const HardwareInfo hwInfo = *platformDevices[0];

    CommandStreamReceiverType csrType = GetParam();

    overrideCommandStreamReceiverCreation = true;
    DebugManager.flags.SetCommandStreamReceiver.set(csrType);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(createCommandStream(&hwInfo, executionEnvironment)));
    if (csrType < CommandStreamReceiverType::CSR_TYPES_NUM) {
        EXPECT_NE(nullptr, executionEnvironment.commandStreamReceivers[0u].get());
        executionEnvironment.memoryManager.reset(executionEnvironment.commandStreamReceivers[0u]->createMemoryManager(false, false));
        EXPECT_NE(nullptr, executionEnvironment.memoryManager.get());
    } else {
        EXPECT_EQ(nullptr, executionEnvironment.commandStreamReceivers[0u]);
        EXPECT_EQ(nullptr, executionEnvironment.memoryManager.get());
    }
}

static CommandStreamReceiverType commandStreamReceiverTypes[] = {
    CSR_HW,
    CSR_AUB,
    CSR_TBX,
    CSR_HW_WITH_AUB,
    CSR_TBX_WITH_AUB,
    CSR_TYPES_NUM};

INSTANTIATE_TEST_CASE_P(
    CreateCommandStreamReceiverTest_Create,
    CreateCommandStreamReceiverTest,
    testing::ValuesIn(commandStreamReceiverTypes));
