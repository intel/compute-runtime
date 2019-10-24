/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/platform/platform.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/tests_configuration.h"

namespace NEO {
struct MockAubCenterFixture {

    MockAubCenterFixture() = default;
    MockAubCenterFixture(CommandStreamReceiverType commandStreamReceiverType) : commandStreamReceiverType(commandStreamReceiverType){};

    void SetUp() {
        setMockAubCenter(platformImpl->peekExecutionEnvironment(), commandStreamReceiverType);
    }
    void TearDown() {
    }

    static void setMockAubCenter(ExecutionEnvironment *executionEnvironment) {
        setMockAubCenter(executionEnvironment, CommandStreamReceiverType::CSR_AUB);
    }
    static void setMockAubCenter(ExecutionEnvironment *executionEnvironment, CommandStreamReceiverType commandStreamReceiverType) {
        if (testMode != TestMode::AubTests && testMode != TestMode::AubTestsWithTbx) {
            auto mockAubCenter = std::make_unique<MockAubCenter>(platformDevices[0], false, "", commandStreamReceiverType);
            mockAubCenter->aubManager = std::make_unique<MockAubManager>();
            executionEnvironment->rootDeviceEnvironments[0].aubCenter.reset(mockAubCenter.release());
        }
    }

  protected:
    CommandStreamReceiverType commandStreamReceiverType = CommandStreamReceiverType::CSR_AUB;
};
} // namespace NEO
