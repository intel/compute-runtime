/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "test.h"

#include "drm/i915_drm.h"

using namespace NEO;

TEST(DrmMapperTests, engineNodeMapPass) {
    unsigned int rcsFlag = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    unsigned int bcsFlag = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    unsigned int expectedRcsFlag = I915_EXEC_RENDER;
    unsigned int expectedBcsFlag = I915_EXEC_BLT;
    EXPECT_EQ(expectedRcsFlag, rcsFlag);
    EXPECT_EQ(expectedBcsFlag, bcsFlag);
}

TEST(DrmMapperTests, engineNodeMapNegative) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
