/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

namespace NEO {

struct GetDeviceInfoMemCapabilitiesTest : ::testing::Test {
    struct TestParams {
        cl_uint paramName;
        cl_unified_shared_memory_capabilities_intel expectedCapabilities;
    };

    void check(std::vector<TestParams> &params) {
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

        for (auto &param : params) {
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

struct QueueFamilyNameTest : ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    }

    void verify(EngineGroupType type, const char *expectedName) {
        char name[CL_QUEUE_FAMILY_MAX_NAME_SIZE_INTEL];
        device->getQueueFamilyName(name, type);
        EXPECT_EQ(0, std::strcmp(name, expectedName));
    }

    std::unique_ptr<MockClDevice> device = {};
};

} // namespace NEO
