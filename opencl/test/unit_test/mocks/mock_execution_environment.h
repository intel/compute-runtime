/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "opencl/test/unit_test/fixtures/mock_aub_center_fixture.h"

namespace NEO {

struct MockRootDeviceEnvironment : public RootDeviceEnvironment {
    using RootDeviceEnvironment::RootDeviceEnvironment;

    ~MockRootDeviceEnvironment() override = default;

    void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) override {
        if (!initAubCenterCalled) {
            initAubCenterCalled = true;
            localMemoryEnabledReceived = localMemoryEnabled;
            aubFileNameReceived = aubFileName;
        }
        if (useMockAubCenter) {
            MockAubCenterFixture::setMockAubCenter(*this);
        }
        RootDeviceEnvironment::initAubCenter(localMemoryEnabled, aubFileName, csrType);
    }
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
    bool useMockAubCenter = true;
};

struct MockExecutionEnvironment : ExecutionEnvironment {
    ~MockExecutionEnvironment() override = default;
    MockExecutionEnvironment() : MockExecutionEnvironment(platformDevices[0]) {}
    MockExecutionEnvironment(const HardwareInfo *hwInfo) : MockExecutionEnvironment(hwInfo, true, 1u) {
    }
    MockExecutionEnvironment(const HardwareInfo *hwInfo, bool useMockAubCenter, uint32_t numRootDevices) {
        prepareRootDeviceEnvironments(numRootDevices);
        for (auto rootDeviceIndex = 0u; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
            auto rootDeviceEnvironment = new MockRootDeviceEnvironment(*this);
            rootDeviceEnvironment->useMockAubCenter = useMockAubCenter;
            rootDeviceEnvironments[rootDeviceIndex].reset(rootDeviceEnvironment);
        }

        if (hwInfo) {
            setHwInfo(hwInfo);
        } else {
            setHwInfo(platformDevices[0]);
        }
        calculateMaxOsContextCount();
    }
};

} // namespace NEO
