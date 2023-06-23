/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

namespace NEO {

struct MockRootDeviceEnvironment : public RootDeviceEnvironment {
    using RootDeviceEnvironment::hwInfo;
    using RootDeviceEnvironment::isDummyAllocationInitialized;
    using RootDeviceEnvironment::RootDeviceEnvironment;
    ~MockRootDeviceEnvironment() override = default;

    void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) override;
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
    bool useMockAubCenter = true;
};

struct MockExecutionEnvironment : ExecutionEnvironment {
    using ExecutionEnvironment::directSubmissionController;

    ~MockExecutionEnvironment() override = default;
    MockExecutionEnvironment();
    MockExecutionEnvironment(const HardwareInfo *hwInfo);
    MockExecutionEnvironment(const HardwareInfo *hwInfo, bool useMockAubCenter, uint32_t numRootDevices);
    void initGmm();
};

} // namespace NEO
