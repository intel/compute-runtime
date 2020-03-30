/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/mock_execution_environment_gmm_fixture.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"

namespace NEO {
void MockExecutionEnvironmentGmmFixture::SetUp() {
    executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->incRefInternal();
}
void MockExecutionEnvironmentGmmFixture::TearDown() {}

GmmHelper *MockExecutionEnvironmentGmmFixture::getGmmHelper() {
    return executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
}

GmmClientContext *MockExecutionEnvironmentGmmFixture::getGmmClientContext() {
    return executionEnvironment->rootDeviceEnvironments[0]->getGmmClientContext();
}
} // namespace NEO