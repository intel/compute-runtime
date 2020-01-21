/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/helpers/ult_hw_config.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_device.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DeviceGenEngineTest, givenNonHwCsrModeWhenGetEngineThenDefaultEngineIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));

    auto &internalEngine = device->getInternalEngine();
    auto &defaultEngine = device->getDefaultEngine();
    EXPECT_EQ(defaultEngine.commandStreamReceiver, internalEngine.commandStreamReceiver);
}
