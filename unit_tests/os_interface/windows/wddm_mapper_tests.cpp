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
    GPUNODE_ORDINAL gpuNode = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    GPUNODE_ORDINAL expected = GPUNODE_3D;
    EXPECT_EQ(expected, gpuNode);
}

TEST(WddmMapperTests, givenNotSupportedEngineWhenAskedForNodeThenAbort) {
    EXPECT_THROW(WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS), std::exception);
}
