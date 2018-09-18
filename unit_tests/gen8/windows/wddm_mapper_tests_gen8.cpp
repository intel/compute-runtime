/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/helpers/gtest_helpers.h"
#include "test.h"
#include "hw_cmds.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"

using namespace OCLRT;
using namespace std;

struct WddmMapperTestsGen8 : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

GEN8TEST_F(WddmMapperTestsGen8, engineNodeMapPass) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<BDWFamily>::engineNodeMap(EngineType::ENGINE_RCS, gpuNode);
    EXPECT_TRUE(ret);
    EXPECT_EQ(GPUNODE_3D, gpuNode);
}

GEN8TEST_F(WddmMapperTestsGen8, engineNodeMapNegative) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<BDWFamily>::engineNodeMap(EngineType::ENGINE_BCS, gpuNode);
    EXPECT_FALSE(ret);
}
