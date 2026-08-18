/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <cstring>

namespace NEO {
namespace LEO {
namespace ult {

struct ClDeviceInfoWindowsTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();

        auto &rootDeviceEnvironment = clDevice->getDevice().getRootDeviceEnvironmentRef();
        rootDeviceEnvironment.osInterface.reset(new OSInterface);
        wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    }

    ClDevice *clDevice = nullptr;
    WddmMock *wddm = nullptr;
};

TEST_F(ClDeviceInfoWindowsTest, givenLuidParamWhenQueryingSizeThenReturnsLuidSize) {
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, 0, nullptr, &retSize));
    EXPECT_EQ(static_cast<size_t>(CL_LUID_SIZE_KHR), retSize);
}

TEST_F(ClDeviceInfoWindowsTest, givenLuidParamWhenQueryingValueThenReturnsAdapterLuid) {
    std::array<uint8_t, CL_LUID_SIZE_KHR> queried{};
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, queried.size(), queried.data(), &retSize));
    EXPECT_EQ(static_cast<size_t>(CL_LUID_SIZE_KHR), retSize);

    const auto adapterLuid = wddm->getAdapterLuid();
    EXPECT_EQ(0, std::memcmp(queried.data(), &adapterLuid, CL_LUID_SIZE_KHR));
}

TEST_F(ClDeviceInfoWindowsTest, givenLuidValidParamWhenQueryingThenReturnsTrue) {
    cl_bool isValid = CL_FALSE;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_LUID_VALID_KHR, sizeof(isValid), &isValid, &retSize));
    EXPECT_EQ(sizeof(cl_bool), retSize);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), isValid);
}

TEST_F(ClDeviceInfoWindowsTest, givenNodeMaskParamWhenQueryingSizeThenReturnsUintSize) {
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, 0, nullptr, &retSize));
    EXPECT_EQ(sizeof(cl_uint), retSize);
}

TEST_F(ClDeviceInfoWindowsTest, givenNodeMaskParamWhenQueryingValueThenReturnsFirstNode) {
    cl_uint nodeMask = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, sizeof(nodeMask), &nodeMask, &retSize));
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(1u, nodeMask);
}

TEST_F(ClDeviceInfoWindowsTest, givenTooSmallBufferForLuidWhenQueryingThenReturnsCLInvalidValue) {
    std::array<uint8_t, CL_LUID_SIZE_KHR> queried{};
    EXPECT_EQ(CL_INVALID_VALUE, clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, queried.size() - 1, queried.data(), nullptr));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
