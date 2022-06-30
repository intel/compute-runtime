/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"

#include "shared/test/common/mocks/mock_execution_environment.h"

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