/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "test.h"

#include "drm/i915_drm.h"

using namespace OCLRT;

TEST(DrmMapperTests, engineNodeMapPass) {
    unsigned int flag = DrmEngineMapper::engineNodeMap(EngineType::ENGINE_RCS);
    unsigned int expected = I915_EXEC_RENDER;
    EXPECT_EQ(expected, flag);
}

TEST(DrmMapperTests, engineNodeMapNegative) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(EngineType::ENGINE_BCS), std::exception);
}
