/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
struct HardwareInfo;
} // namespace NEO

using namespace NEO;

struct CreateCommandStreamReceiverTest : public ::testing::TestWithParam<CommandStreamReceiverType> {};

HWTEST_P(CreateCommandStreamReceiverTest, givenCreateCommandStreamWhenCsrIsSetToValidTypeThenTheFuntionReturnsCommandStreamReceiver) {
    DebugManagerStateRestore stateRestorer;
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
    ASSERT_NE(nullptr, executionEnvironment->memoryManager.get());
    executionEnvironment->incRefInternal();
    MockAubCenterFixture::setMockAubCenter(*executionEnvironment->rootDeviceEnvironments[0]);

    CommandStreamReceiverType csrType = GetParam();

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(csrType));
    {
        auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 0, 1));

        if (csrType < CommandStreamReceiverType::typesNum) {
            EXPECT_NE(nullptr, csr.get());
        } else {
            EXPECT_EQ(nullptr, csr.get());
        }
    }

    executionEnvironment->decRefInternal();
}

static CommandStreamReceiverType commandStreamReceiverTypes[] = {
    CommandStreamReceiverType::hardware,
    CommandStreamReceiverType::aub,
    CommandStreamReceiverType::tbx,
    CommandStreamReceiverType::hardwareWithAub,
    CommandStreamReceiverType::tbxWithAub,
    CommandStreamReceiverType::nullAub,
    CommandStreamReceiverType::typesNum};

INSTANTIATE_TEST_SUITE_P(
    CreateCommandStreamReceiverTest_Create,
    CreateCommandStreamReceiverTest,
    testing::ValuesIn(commandStreamReceiverTypes));
