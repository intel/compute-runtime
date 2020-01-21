/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_engine_mapper.h"
#include "test.h"

#include "drm/i915_drm.h"

using namespace NEO;

TEST(DrmMapperTests, engineNodeMapPass) {
    unsigned int rcsFlag = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    unsigned int bcsFlag = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    unsigned int ccsFlag = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS);
    unsigned int expectedRcsFlag = I915_EXEC_RENDER;
    unsigned int expectedBcsFlag = I915_EXEC_BLT;
    unsigned int expectedCcsFlag = I915_EXEC_COMPUTE;
    EXPECT_EQ(expectedRcsFlag, rcsFlag);
    EXPECT_EQ(expectedBcsFlag, bcsFlag);
    EXPECT_EQ(expectedCcsFlag, ccsFlag);
}

TEST(DrmMapperTests, engineNodeMapNegative) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
