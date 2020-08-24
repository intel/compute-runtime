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
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    EXPECT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}
TEST(EngineInfoTest, givenEngineInfoQuerySupportedWhenQueryingEngineInfoThenEngineInfoIsCreatedWithEngines) {
    auto drm = std::make_unique<DrmMockEngine>();
    ASSERT_NE(nullptr, drm);

    drm->queryEngineInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = static_cast<EngineInfoImpl *>(drm->getEngineInfo());

    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(2u, engineInfo->engines.size());
}