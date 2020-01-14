/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace NEO {

struct GetDeviceInfoMemCapabilitiesTest : ::testing::Test {
    struct TestParams {
        cl_uint paramName;
        cl_unified_shared_memory_capabilities_intel expectedCapabilities;
    };

    void check(std::vector<TestParams> &params) {
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

        for (auto param : params) {
            cl_unified_shared_memory_capabilities_intel unifiedSharedMemoryCapabilities{};
            size_t paramRetSize;

            const auto retVal = device->getDeviceInfo(param.paramName,
                                                      sizeof(cl_unified_shared_memory_capabilities_intel),
                                                      &unifiedSharedMemoryCapabilities, &paramRetSize);
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_EQ(param.expectedCapabilities, unifiedSharedMemoryCapabilities);
            EXPECT_EQ(sizeof(cl_unified_shared_memory_capabilities_intel), paramRetSize);
        }
    }
};
} // namespace NEO
