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

struct WddmMapperTestsGen10 : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

GEN10TEST_F(WddmMapperTestsGen10, engineNodeMapPass) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<CNLFamily>::engineNodeMap(EngineType::ENGINE_RCS, gpuNode);
    EXPECT_TRUE(ret);
    EXPECT_EQ(GPUNODE_3D, gpuNode);
}

GEN10TEST_F(WddmMapperTestsGen10, engineNodeMapNegative) {
    GPUNODE_ORDINAL gpuNode = GPUNODE_MAX;
    bool ret = WddmEngineMapper<CNLFamily>::engineNodeMap(EngineType::ENGINE_BCS, gpuNode);
    EXPECT_FALSE(ret);
}
