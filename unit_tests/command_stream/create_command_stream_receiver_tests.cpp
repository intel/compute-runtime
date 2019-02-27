/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/libult/create_command_stream.h"

using namespace OCLRT;

struct CreateCommandStreamReceiverTest : public ::testing::TestWithParam<CommandStreamReceiverType> {};

HWTEST_P(CreateCommandStreamReceiverTest, givenCreateCommandStreamWhenCsrIsSetToValidTypeThenTheFuntionReturnsCommandStreamReceiver) {
    DebugManagerStateRestore stateRestorer;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo);

    CommandStreamReceiverType csrType = GetParam();

    VariableBackup<bool> backup(&overrideCommandStreamReceiverCreation, true);
    DebugManager.flags.SetCommandStreamReceiver.set(csrType);
    executionEnvironment->commandStreamReceivers.resize(1);
    executionEnvironment->commandStreamReceivers[0].push_back(std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment)));

    if (csrType < CommandStreamReceiverType::CSR_TYPES_NUM) {
        EXPECT_NE(nullptr, executionEnvironment->commandStreamReceivers[0][0].get());
    } else {
        EXPECT_EQ(nullptr, executionEnvironment->commandStreamReceivers[0][0]);
    }
    EXPECT_NE(nullptr, executionEnvironment->memoryManager.get());
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
