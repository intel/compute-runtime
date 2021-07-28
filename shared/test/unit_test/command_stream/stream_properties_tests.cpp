/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

#include "test.h"

namespace NEO {

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.isCoherencyRequired);
    allProperties.push_back(&properties.largeGrfMode);
    return allProperties;
}

std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.disableOverdispatch);
    return allProperties;
}

} // namespace NEO

using namespace NEO;

TEST(StreamPropertiesTests, whenSettingCooperativeKernelPropertiesThenCorrectValueIsSet) {
    StreamProperties properties;
    for (auto disableOverdispatch : ::testing::Bool()) {
        properties.frontEndState.setProperties(false, disableOverdispatch, *defaultHwInfo);
        EXPECT_EQ(disableOverdispatch, properties.frontEndState.disableOverdispatch.value);
    }
}
