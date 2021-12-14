/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_engine_mapper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(WddmMapperTests, givenRcsEngineTypeWhenAskedForNodeOrdinalThenReturn3d) {
    GPUNODE_ORDINAL gpuNodeBcs = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    GPUNODE_ORDINAL gpuNodeRcs = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    GPUNODE_ORDINAL gpuNodeCcs = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS);
    GPUNODE_ORDINAL gpuNodeCcs1 = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS1);
    GPUNODE_ORDINAL gpuNodeCcs2 = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS2);
    GPUNODE_ORDINAL gpuNodeCcs3 = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS3);
    GPUNODE_ORDINAL gpuNodeCccs = WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCCS);

    GPUNODE_ORDINAL expectedBcs = GPUNODE_BLT;
    GPUNODE_ORDINAL expectedRcs = GPUNODE_3D;
    GPUNODE_ORDINAL expectedCcs = GPUNODE_CCS0;
    GPUNODE_ORDINAL expectedCccs = GPUNODE_3D;
    EXPECT_EQ(expectedBcs, gpuNodeBcs);
    EXPECT_EQ(expectedRcs, gpuNodeRcs);
    EXPECT_EQ(expectedCcs, gpuNodeCcs);
    EXPECT_EQ(expectedCcs, gpuNodeCcs1);
    EXPECT_EQ(expectedCcs, gpuNodeCcs2);
    EXPECT_EQ(expectedCcs, gpuNodeCcs3);
    EXPECT_EQ(expectedCccs, gpuNodeCccs);
}

TEST(WddmMapperTests, givenLinkCopyEngineWhenMapperCalledThenReturnDefaultBltEngine) {
    const std::array<aub_stream::EngineType, 8> bcsLinkEngines = {{aub_stream::ENGINE_BCS1, aub_stream::ENGINE_BCS2, aub_stream::ENGINE_BCS3,
                                                                   aub_stream::ENGINE_BCS4, aub_stream::ENGINE_BCS5, aub_stream::ENGINE_BCS6,
                                                                   aub_stream::ENGINE_BCS7, aub_stream::ENGINE_BCS8}};

    for (auto engine : bcsLinkEngines) {
        EXPECT_EQ(GPUNODE_BLT, WddmEngineMapper::engineNodeMap(engine));
    }
}

TEST(WddmMapperTests, givenNotSupportedEngineWhenAskedForNodeThenAbort) {
    EXPECT_THROW(WddmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
