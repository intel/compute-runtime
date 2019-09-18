/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_engine_mapper.h"
#include "test.h"

using namespace NEO;

TEST(WddmMapperTests, givenRcsEngineTypeWhenAskedForNodeOrdinalThenReturn3d) {
    GPUNODE_ORDINAL rcsNode = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    GPUNODE_ORDINAL bcsNode = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    GPUNODE_ORDINAL ccsNode = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS);
    GPUNODE_ORDINAL expectedRcsNode = GPUNODE_3D;
    GPUNODE_ORDINAL expectedBcsNode = GPUNODE_BLT;
    GPUNODE_ORDINAL expectedCcsNode = GPUNODE_CCS0;
    EXPECT_EQ(expectedRcsNode, rcsNode);
    EXPECT_EQ(expectedBcsNode, bcsNode);
    EXPECT_EQ(expectedCcsNode, ccsNode);
}

TEST(WddmMapperTests, givenNotSupportedEngineWhenAskedForNodeThenAbort) {
    EXPECT_THROW(WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
