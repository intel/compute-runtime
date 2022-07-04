/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/test/common/test_macros/test.h"

#include "engine_node.h"

#include <array>

using namespace NEO;

TEST(DrmMapperTests, GivenEngineWhenMappingNodeThenCorrectEngineReturned) {
    auto flagBcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_BCS);
    auto flagRcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_RCS);
    auto flagCcs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS);
    auto flagCccs = DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCCS);
    auto expectedBcs = DrmParam::ExecBlt;
    auto expectedRcs = DrmParam::ExecRender;
    auto expectedCcs = DrmParam::ExecDefault;
    auto expectedCccs = DrmParam::ExecRender;
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
        EXPECT_EQ(DrmParam::ExecBlt, DrmEngineMapper::engineNodeMap(engine));
    }
}

TEST(DrmMapperTests, GivenCcsWhenGettingEngineNodeMapThenReturnDefault) {
    auto expected = DrmParam::ExecDefault;
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS1), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS2), expected);
    EXPECT_EQ(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_CCS3), expected);
}

TEST(DrmMapperTests, GivenVcsWhenGettingEngineNodeMapThenExceptionIsThrown) {
    EXPECT_THROW(DrmEngineMapper::engineNodeMap(aub_stream::ENGINE_VCS), std::exception);
}
