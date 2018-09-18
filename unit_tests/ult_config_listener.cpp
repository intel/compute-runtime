/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"
#include "unit_tests/ult_config_listener.h"

void OCLRT::UltConfigListener::OnTestStart(const ::testing::TestInfo &testInfo) {
    // Create platform and initialize gmm that dont want to create Platform and test gmm initialization path
    constructPlatform()->peekExecutionEnvironment()->initGmm(*platformDevices);
}
void OCLRT::UltConfigListener::OnTestEnd(const ::testing::TestInfo &testInfo) {
    // Clear global platform that it shouldn't be reused between tests
    platformImpl.reset();
}
