/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/fixtures/mock_aub_center_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/libult/create_command_stream.h"
#include "test.h"

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
