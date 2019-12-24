/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/ult_config_listener.h"

#include "core/helpers/options.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/platform/platform.h"

void NEO::UltConfigListener::OnTestStart(const ::testing::TestInfo &testInfo) {
    auto executionEnvironment = constructPlatform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setHwInfo(*platformDevices);
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->initGmm();
}
void NEO::UltConfigListener::OnTestEnd(const ::testing::TestInfo &testInfo) {
    // Clear global platform that it shouldn't be reused between tests
    platformImpl.reset();
    MemoryManager::maxOsContextCount = 0u;
}
