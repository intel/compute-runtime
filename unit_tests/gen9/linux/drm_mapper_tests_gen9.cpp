/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm/i915_drm.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "unit_tests/helpers/gtest_helpers.h"
#include "test.h"
#include "hw_cmds.h"

using namespace OCLRT;
using namespace std;

struct DrmMapperTestsGen9 : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

GEN9TEST_F(DrmMapperTestsGen9, engineNodeMapPass) {
    unsigned int flag = I915_EXEC_RING_MASK;
    unsigned int expected = I915_EXEC_RENDER;
    bool ret = DrmEngineMapper<SKLFamily>::engineNodeMap(EngineType::ENGINE_RCS, flag);
    EXPECT_TRUE(ret);
    EXPECT_EQ(expected, flag);
}

GEN9TEST_F(DrmMapperTestsGen9, engineNodeMapNegative) {
    unsigned int flag = I915_EXEC_RING_MASK;
    bool ret = DrmEngineMapper<SKLFamily>::engineNodeMap(EngineType::ENGINE_BCS, flag);
    EXPECT_FALSE(ret);
}
