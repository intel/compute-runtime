/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "test.h"
#include "hw_cmds.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"

#include "drm/i915_drm.h"

using namespace OCLRT;
using namespace std;

struct DrmMapperTestsGen10 : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

GEN10TEST_F(DrmMapperTestsGen10, engineNodeMapPass) {
    unsigned int flag = I915_EXEC_RING_MASK;
    unsigned int expected = I915_EXEC_RENDER;
    bool ret = DrmEngineMapper<CNLFamily>::engineNodeMap(EngineType::ENGINE_RCS, flag);
    EXPECT_TRUE(ret);
    EXPECT_EQ(expected, flag);
}

GEN10TEST_F(DrmMapperTestsGen10, engineNodeMapNegative) {
    unsigned int flag = I915_EXEC_RING_MASK;
    bool ret = DrmEngineMapper<CNLFamily>::engineNodeMap(EngineType::ENGINE_BCS, flag);
    EXPECT_FALSE(ret);
}
