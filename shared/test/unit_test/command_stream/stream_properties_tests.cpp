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
    allProperties.push_back(&properties.zPassAsyncComputeThreadLimit);
    allProperties.push_back(&properties.pixelAsyncComputeThreadLimit);
    allProperties.push_back(&properties.threadArbitrationPolicy);
    return allProperties;
}

std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.disableOverdispatch);
    allProperties.push_back(&properties.singleSliceDispatchCcsMode);
    allProperties.push_back(&properties.computeDispatchAllWalkerEnable);
    return allProperties;
}

} // namespace NEO
