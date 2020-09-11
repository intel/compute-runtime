/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info_impl.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenQueryingEngineInfoThenSingleIoctlIsCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}
TEST(EngineInfoTest, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = static_cast<EngineInfoImpl *>(drm->getEngineInfo());

    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->engines.size());
}
