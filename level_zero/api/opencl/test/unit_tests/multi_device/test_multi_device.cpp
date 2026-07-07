/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "CL/cl.h"

#include <limits>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct MultiDeviceOclFixture : public L0::ult::MultiDeviceFixture {
    void setUp() {
        L0::ult::MultiDeviceFixture::setUp();
        platform = std::make_unique<Platform>(driverHandle->toHandle());
    }

    void tearDown() {
        platform.reset();
        L0::ult::MultiDeviceFixture::tearDown();
    }

    std::vector<cl_device_id> getClDeviceIds() {
        std::vector<cl_device_id> devices;
        for (auto &clDevice : platform->getDevices()) {
            devices.push_back(clDevice.get());
        }
        return devices;
    }

    std::unique_ptr<Platform> platform;
};

using LeoMultiDevicePlatformTests = Test<MultiDeviceOclFixture>;

TEST_F(LeoMultiDevicePlatformTests, givenMultiRootDeviceDriverWhenCreatingPlatformThenAllRootDevicesAreExposed) {
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getDevices().size()));
}

TEST_F(LeoMultiDevicePlatformTests, givenMultiRootDeviceDriverWhenCreatingPlatformThenRootDeviceIndicesAndBitfieldsAreGrouped) {
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getRootDeviceIndices().size()));
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(platform->getDeviceBitfields().size()));

    for (auto &clDevice : platform->getDevices()) {
        EXPECT_FALSE(clDevice->getIsSubdevice());
        EXPECT_NE(platform->getDeviceBitfields().end(), platform->getDeviceBitfields().find(clDevice->getRootDeviceIndex()));
    }
}

TEST_F(LeoMultiDevicePlatformTests, givenMultiDevicePlatformWhenClGetDeviceIDsThenAllRootDevicesAreReturned) {
    cl_uint numDevices = 0;
    auto retVal = clGetDeviceIDs(platform.get(), CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numRootDevices, numDevices);

    std::vector<cl_device_id> devices(numDevices, nullptr);
    retVal = clGetDeviceIDs(platform.get(), CL_DEVICE_TYPE_GPU, numDevices, devices.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto device : devices) {
        EXPECT_NE(nullptr, device);
    }
}

using LeoMultiDeviceContextTests = Test<MultiDeviceOclFixture>;

TEST_F(LeoMultiDeviceContextTests, givenMultipleDevicesWhenCreatingContextThenItIsNotSingleDeviceAndGroupsAllRootDevices) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, static_cast<cl_uint>(clDevices.size()), clDevices.data(), true);

    EXPECT_FALSE(clContext.isSingleDeviceContext());
    EXPECT_EQ(clDevices.size(), clContext.getClDevices().size());
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(clContext.getRootDeviceIndices().size()));
    EXPECT_EQ(numRootDevices, static_cast<uint32_t>(clContext.getDeviceBitfields().size()));
}

TEST_F(LeoMultiDeviceContextTests, givenMultiDeviceContextWhenGettingClDeviceByRootDeviceIndexThenMatchingDeviceIsReturned) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, static_cast<cl_uint>(clDevices.size()), clDevices.data(), true);

    for (auto clDeviceId : clDevices) {
        auto clDevice = castToObject<ClDevice>(clDeviceId);
        EXPECT_EQ(clDevice, clContext.getClDeviceByRootDeviceIndex(clDevice->getRootDeviceIndex()));
    }
    EXPECT_EQ(nullptr, clContext.getClDeviceByRootDeviceIndex(std::numeric_limits<uint32_t>::max()));
}

TEST_F(LeoMultiDeviceContextTests, givenSingleDeviceWhenCreatingContextThenItIsSingleDeviceContextWithThatDeviceAsDefault) {
    auto clDevices = getClDeviceIds();
    Context clContext(nullptr, nullptr, 1u, clDevices.data(), true);

    EXPECT_TRUE(clContext.isSingleDeviceContext());
    EXPECT_EQ(1u, clContext.getRootDeviceIndices().size());
    EXPECT_EQ(clContext.getClDevices()[0]->getRootDeviceIndex(), clContext.getDefaultRootDeviceIndex());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
