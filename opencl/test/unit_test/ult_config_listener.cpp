/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/ult_config_listener.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

void NEO::UltConfigListener::OnTestStart(const ::testing::TestInfo &testInfo) {
    BaseUltConfigListener::OnTestStart(testInfo);

    auto executionEnvironment = constructPlatform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
}

void NEO::UltConfigListener::OnTestEnd(const ::testing::TestInfo &testInfo) {
    // Clear global platform that it shouldn't be reused between tests
    platformsImpl->clear();
    MemoryManager::maxOsContextCount = 0u;

    BaseUltConfigListener::OnTestEnd(testInfo);
}
