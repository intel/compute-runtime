/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_tests.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_impl.h"

namespace NEO {

class DrmMemoryManagerFixtureImpl : public DrmMemoryManagerFixture {
  public:
    DrmMockCustomImpl *mockExp;

    void SetUp() override {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        MemoryManagementFixture::SetUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        mockExp = new DrmMockCustomImpl(*executionEnvironment->rootDeviceEnvironments[0]);
        DrmMemoryManagerFixture::SetUp(mockExp, true);
    }

    void TearDown() override {
        mockExp->testIoctls();
        DrmMemoryManagerFixture::TearDown();
    }
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};
} // namespace NEO
