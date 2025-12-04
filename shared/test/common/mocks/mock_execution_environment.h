/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include <optional>

namespace NEO {

class BuiltIns;

struct MockRootDeviceEnvironment : public RootDeviceEnvironment {
    using BaseClass = RootDeviceEnvironment;
    using RootDeviceEnvironment::hwInfo;
    using RootDeviceEnvironment::isDummyAllocationInitialized;
    using RootDeviceEnvironment::isWddmOnLinuxEnable;
    using RootDeviceEnvironment::RootDeviceEnvironment;
    ~MockRootDeviceEnvironment() override;

    void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) override;
    bool initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) override;
    bool initAilConfiguration() override;

    static void resetBuiltins(RootDeviceEnvironment *rootDeviceEnvironment, BuiltIns *newValue);

    std::vector<bool> initOsInterfaceResults;
    uint32_t initOsInterfaceCalled = 0u;
    std::optional<uint32_t> initOsInterfaceExpectedCallCount;
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
    bool useMockAubCenter = true;
    std::optional<bool> ailInitializationResult{true};
};

struct MockExecutionEnvironment : ExecutionEnvironment {
    using ExecutionEnvironment::adjustCcsCountImpl;
    using ExecutionEnvironment::configureCcsMode;
    using ExecutionEnvironment::directSubmissionController;
    using ExecutionEnvironment::memoryManager;
    using ExecutionEnvironment::rootDeviceEnvironments;

    ~MockExecutionEnvironment() override = default;
    MockExecutionEnvironment();
    MockExecutionEnvironment(const HardwareInfo *hwInfo);
    MockExecutionEnvironment(const HardwareInfo *hwInfo, bool useMockAubCenter, uint32_t numRootDevices);
    void initGmm();
};

} // namespace NEO
