/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_exp.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"

namespace NEO {

class DrmMemoryManagerFixtureExp : public DrmMemoryManagerFixture {
  public:
    DrmMockCustomExp *mockExp;

    void SetUp() override {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        MemoryManagementFixture::SetUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        mockExp = new DrmMockCustomExp(*executionEnvironment->rootDeviceEnvironments[0]);
        DrmMemoryManagerFixture::SetUp(mockExp, true);
    }

    void TearDown() override {
        mockExp->testIoctlsExp();
        DrmMemoryManagerFixture::TearDown();
    }
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};
} // namespace NEO
