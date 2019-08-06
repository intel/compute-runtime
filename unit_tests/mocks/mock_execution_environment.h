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
    bool initAubCenterCalled = false;
    bool localMemoryEnabledReceived = false;
    std::string aubFileNameReceived = "";
    bool useMockAubCenter = true;
};

template <typename CsrType>
struct MockExecutionEnvironmentWithCsr : public ExecutionEnvironment {
    MockExecutionEnvironmentWithCsr() = delete;
    MockExecutionEnvironmentWithCsr(const HardwareInfo &hwInfo, uint32_t devicesCount) {
        setHwInfo(&hwInfo);
        auto &gpgpuEngines = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances();
        commandStreamReceivers.resize(devicesCount);

        for (uint32_t csrIndex = 0; csrIndex < gpgpuEngines.size(); csrIndex++) {
            for (auto &csr : commandStreamReceivers) {
                csr.push_back(std::unique_ptr<CommandStreamReceiver>(new CsrType(*this)));
            }
        }
    }
};
} // namespace NEO
