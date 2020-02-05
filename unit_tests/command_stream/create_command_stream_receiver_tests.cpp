/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/helpers/ult_hw_config.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/libult/create_command_stream.h"

using namespace NEO;

struct CreateCommandStreamReceiverTest : public ::testing::TestWithParam<CommandStreamReceiverType> {};

HWTEST_P(CreateCommandStreamReceiverTest, givenCreateCommandStreamWhenCsrIsSetToValidTypeThenTheFuntionReturnsCommandStreamReceiver) {
    DebugManagerStateRestore stateRestorer;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);

    CommandStreamReceiverType csrType = GetParam();

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    DebugManager.flags.SetCommandStreamReceiver.set(csrType);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 0));

    if (csrType < CommandStreamReceiverType::CSR_TYPES_NUM) {
        EXPECT_NE(nullptr, csr.get());
    } else {
        EXPECT_EQ(nullptr, csr.get());
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
