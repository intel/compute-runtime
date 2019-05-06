/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
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
} // namespace NEO
