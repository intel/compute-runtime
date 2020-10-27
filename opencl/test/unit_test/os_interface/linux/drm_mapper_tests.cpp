/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"

#include "test.h"

#include "drm/i915_drm.h"

using namespace NEO;

TEST(DrmMapperTests, GivenRcsWhenGettingEngineNodeMapThenExecRenderIsReturned) {
    unsigned int expected = I915_EXEC_RENDER;
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS), expected);
}

TEST(DrmMapperTests, GivenBcsWhenGettingEngineNodeMapThenExecBltIsReturned) {
    unsigned int expected = I915_EXEC_BLT;
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS), expected);
}

TEST(DrmMapperTests, GivenCcsWhenGettingEngineNodeMapThenExceptionIsThrown) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS), std::exception);
}

TEST(DrmMapperTests, GivenVcsWhenGettingEngineNodeMapThenExceptionIsThrown) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
