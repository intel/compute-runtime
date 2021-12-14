/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

struct MultiTileFixture : public ::testing::Test {
    void SetUp() override {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        ultHwConfig.useHwCsr = true;
        ultHwConfig.forceOsAgnosticMemoryManager = false;
        DebugManager.flags.CreateMultipleSubDevices.set(requiredDeviceCount);
        DebugManager.flags.DeferOsContextInitialization.set(0);
        platformsImpl->clear();
        constructPlatform();
        initPlatform();
    };

  protected:
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    DebugManagerStateRestore stateRestore;
    cl_uint requiredDeviceCount = 2u;
};

struct FourTileFixture : public MultiTileFixture {
    FourTileFixture() : MultiTileFixture() { requiredDeviceCount = 4; }
};
