/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/fan/linux/mock_fan.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t fanHandleComponentCount = 0u;
class SysmanDeviceFanFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<L0::Sysman::Fan> pFan;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pFan = std::make_unique<L0::Sysman::FanImp>(pOsSysman, 0, false);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_fan_handle_t> getFanHandles(uint32_t count) {
        std::vector<zes_fan_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFanDomainsThenValidCountIsReturnedAndVerifySysmanFanGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);
}

TEST_F(SysmanDeviceFanFixture, GivenInvalidComponentCountWhenEnumeratingFanDomainsThenValidCountIsReturnedAndVerifySysmanFanGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);
}

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFanDomainsThenValidFanHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, fanHandleComponentCount);

    std::vector<zes_fan_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingFixedSpeedModeThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_t fanSpeed = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetFixedSpeedMode(fanHandle, &fanSpeed));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithPercentUnitThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;
    int32_t fanSpeed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(fanHandle, unit, &fanSpeed));
}

TEST_F(SysmanDeviceFanFixture, GivenFanHandleContextWhenCallingCreateHandleThenHandleIsCreatedSuccessfully) {
    // Create a FanHandleContext to directly test the createHandle functionality
    auto fanContext = std::make_unique<L0::Sysman::FanHandleContext>(pOsSysman);

    // Test that createHandle works correctly by calling it directly
    uint32_t fanIndex = 0;
    bool multipleFansSupported = false;

    // Verify initial handle count is 0
    EXPECT_EQ(fanContext->handleList.size(), 0u);

    // Call createHandle directly to test the function
    fanContext->createHandle(fanIndex, multipleFansSupported);

    // Verify handle was created
    EXPECT_EQ(fanContext->handleList.size(), 1u);
    EXPECT_NE(fanContext->handleList[0], nullptr);
}

TEST_F(SysmanDeviceFanFixture, GivenFanHandleContextWhenCallingFanGetThenCorrectCountOfFanHandlesIsReturned) {
    uint32_t fanCount = 0;
    auto fanContext = std::make_unique<MockFanHandleContext>(pOsSysman);

    fanContext->mockSupportedFanCount = 2;
    fanContext->init();

    EXPECT_EQ(ZE_RESULT_SUCCESS, fanContext->fanGet(&fanCount, nullptr));
    EXPECT_EQ(2u, fanCount);

    std::vector<zes_fan_handle_t> handles(fanCount, nullptr);
    fanCount += 1;

    EXPECT_EQ(ZE_RESULT_SUCCESS, fanContext->fanGet(&fanCount, handles.data()));
    EXPECT_EQ(2u, fanCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
