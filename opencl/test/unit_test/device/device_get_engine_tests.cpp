/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/options.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

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
