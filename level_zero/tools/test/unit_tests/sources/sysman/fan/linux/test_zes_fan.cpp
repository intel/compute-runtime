/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/fan_imp.h"
#include "level_zero/tools/source/sysman/fan/linux/os_fan_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t fanHandleComponentCount = 0u;
class SysmanDeviceFanFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<L0::Fan> pFan;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFan = std::make_unique<L0::FanImp>(pOsSysman);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
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

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesThenCallSucceeds) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_properties_t properties;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetProperties(fanHandle, &properties));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_config_t fanConfig;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetConfig(fanHandle, &fanConfig));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingDefaultModeThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetDefaultMode(fanHandle));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingFixedSpeedModeThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_t fanSpeed = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetFixedSpeedMode(fanHandle, &fanSpeed));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingTheSpeedTableModeThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_table_t fanSpeedTable = {0};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetSpeedTableMode(fanHandle, &fanSpeedTable));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithRPMUnitThenValidFanSpeedReadingsRetrieved) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
    int32_t fanSpeed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(fanHandle, unit, &fanSpeed));
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithPercentUnitThenUnsupportedIsReturned) {
    zes_fan_handle_t fanHandle = pFan->toHandle();
    zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;
    int32_t fanSpeed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(fanHandle, unit, &fanSpeed));
}

} // namespace ult
} // namespace L0
