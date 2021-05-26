/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

namespace NEO {

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.isCoherencyRequired);
    return allProperties;
}

std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties) {
    return {};
}

} // namespace NEO
