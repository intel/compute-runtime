/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"

namespace NEO {
struct MockExecutionEnvironment : ExecutionEnvironment {
    MockExecutionEnvironment() = default;
    MockExecutionEnvironment(const HardwareInfo *hwInfo) : MockExecutionEnvironment(hwInfo, true) {}
    MockExecutionEnvironment(const HardwareInfo *hwInfo, bool useMockAubCenter) : useMockAubCenter(useMockAubCenter) {
        if (hwInfo) {
            setHwInfo(hwInfo);
        }
    }
    void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) override {
        if (!initAubCenterCalled) {
            initAubCenterCalled = true;
            localMemoryEnabledReceived = localMemoryEnabled;
            aubFileNameReceived = aubFileName;
        }
        if (useMockAubCenter) {
            MockAubCenterFixture::setMockAubCenter(this);
        }
        ExecutionEnvironment::initAubCenter(localMemoryEnabled, aubFileName, csrType);
    }
    bool initializeRootCommandStreamReceiver(RootDevice &device) override {
        return initRootCommandStreamReceiver;
    }
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
    bool useMockAubCenter = true;
    bool initRootCommandStreamReceiver = false;
};

template <typename CsrType>
struct MockExecutionEnvironmentWithCsr : public ExecutionEnvironment {
    MockExecutionEnvironmentWithCsr() = delete;
    MockExecutionEnvironmentWithCsr(const HardwareInfo &hwInfo, uint32_t devicesCount) {
        setHwInfo(&hwInfo);
        auto &gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances();
        auto offset = devicesCount > 1 ? 1u : 0u;
        rootDeviceEnvironments[0].commandStreamReceivers.resize(devicesCount + offset);

        for (uint32_t deviceIndex = 0; deviceIndex < devicesCount; deviceIndex++) {
            for (uint32_t csrIndex = 0; csrIndex < gpgpuEngines.size(); csrIndex++) {
                rootDeviceEnvironments[0].commandStreamReceivers[deviceIndex + offset].push_back(std::unique_ptr<CommandStreamReceiver>(new CsrType(*this, 0)));
            }
        }
    }
};
} // namespace NEO
