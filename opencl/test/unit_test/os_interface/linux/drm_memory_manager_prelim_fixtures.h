/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_prelim.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_memory_info.h"

#include "gtest/gtest.h"

class DrmMemoryManagerLocalMemoryPrelimTest : public ::testing::Test {
  public:
    DrmQueryMock *mock;

    void SetUp() override {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());

        mock = new DrmQueryMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        mock->memoryInfo.reset(new MockExtendedMemoryInfo());

        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
        constexpr bool localMemoryEnabled = true;
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    const uint32_t rootDeviceIndex = 0u;
};

class DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest : public ::testing::Test {
  public:
    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        mock = new DrmMockCustomPrelim(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

  protected:
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustomPrelim *mock;
    ExecutionEnvironment *executionEnvironment;
};
