/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
