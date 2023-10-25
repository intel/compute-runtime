/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

namespace ULT {

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoLuidSizeThenCorrectAdapterLuidSizeIsReturned) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    size_t sizeReturned = std::numeric_limits<size_t>::max();
    size_t expectedSize = CL_LUID_SIZE_KHR;
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, 0, nullptr, &sizeReturned);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(sizeReturned, expectedSize);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenCorrectAdapterLuidIsReturned) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    std::array<uint8_t, CL_LUID_SIZE_KHR> deviceLuidKHR;
    auto luid = clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>()->getAdapterLuid();
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, CL_LUID_SIZE_KHR, deviceLuidKHR.data(), 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(std::memcmp(deviceLuidKHR.data(), &luid, CL_LUID_SIZE_KHR), 0);
}

TEST(GetDeviceInfo, givenClDeviceWhenVerifyAdapterLuidThenGetTrue) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    cl_bool isValid = false;
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_VALID_KHR, sizeof(cl_bool), &isValid, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(isValid);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoNodeMaskSizeThenCorrectNodeMaskSizeIsReturned) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    size_t sizeReturned = std::numeric_limits<size_t>::max();
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, 0, nullptr, &sizeReturned);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(sizeReturned, sizeof(cl_uint));
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenGetNodeMask) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    cl_uint nodeMask = 0b0;
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, sizeof(cl_uint), &nodeMask, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(nodeMask, 0b1u);
}
} // namespace ULT
