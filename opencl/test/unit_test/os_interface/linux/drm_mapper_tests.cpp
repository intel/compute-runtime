/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/test/common/test_macros/test.h"

#include "drm/i915_drm.h"

using namespace NEO;

TEST(DrmMapperTests, GivenEngineWhenMappingNodeThenCorrectEngineReturned) {
    unsigned int flagBcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    unsigned int flagRcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    unsigned int flagCcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS);
    unsigned int flagCccs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCCS);
    unsigned int expectedBcs = I915_EXEC_BLT;
    unsigned int expectedRcs = I915_EXEC_RENDER;
    unsigned int expectedCcs = I915_EXEC_DEFAULT;
    unsigned int expectedCccs = I915_EXEC_RENDER;
    EXPECT_EQ(expectedBcs, flagBcs);
    EXPECT_EQ(expectedRcs, flagRcs);
    EXPECT_EQ(expectedCcs, flagCcs);
    EXPECT_EQ(expectedCccs, flagCccs);
}

TEST(DrmMapperTests, givenLinkCopyEngineWhenMapperCalledThenReturnDefaultBltEngine) {
    const std::array<aub_stream::EngineType, 8> bcsLinkEngines = {{aub_stream::ENGINE_BCS1, aub_stream::ENGINE_BCS2, aub_stream::ENGINE_BCS3,
                                                                   aub_stream::ENGINE_BCS4, aub_stream::ENGINE_BCS5, aub_stream::ENGINE_BCS6,
                                                                   aub_stream::ENGINE_BCS7, aub_stream::ENGINE_BCS8}};

    for (auto engine : bcsLinkEngines) {
        EXPECT_EQ(static_cast<unsigned int>(I915_EXEC_BLT), DrmEngineMapper::engineNodeMap(engine));
    }
}

TEST(DrmMapperTests, GivenCcsWhenGettingEngineNodeMapThenReturnDefault) {
    unsigned int expected = I915_EXEC_DEFAULT;
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS1), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS2), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS3), expected);
}

TEST(DrmMapperTests, GivenVcsWhenGettingEngineNodeMapThenExceptionIsThrown) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
