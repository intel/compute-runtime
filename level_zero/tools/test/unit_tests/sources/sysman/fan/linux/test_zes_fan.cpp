/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/linux/os_fan_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "sysman/fan/fan_imp.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t fanHandleComponentCount = 0u;
class SysmanDeviceFanFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pSysmanDeviceImp->pFanHandleContext->init();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_fan_handle_t> get_fan_handles(uint32_t count) {
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
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetProperties(handle, &properties));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigThenUnsupportedIsReturned) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetConfig(handle, &fanConfig));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingDefaultModeThenUnsupportedIsReturned) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetDefaultMode(handle));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingFixedSpeedModeThenUnsupportedIsReturned) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_speed_t fanSpeed = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetFixedSpeedMode(handle, &fanSpeed));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingTheSpeedTableModeThenUnsupportedIsReturned) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithRPMUnitThenValidFanSpeedReadingsRetrieved) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(handle, unit, &fanSpeed));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanSpeedWithPercentUnitThenUnsupportedIsReturned) {
    auto handles = get_fan_handles(fanHandleComponentCount);

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;
        int32_t fanSpeed = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(handle, unit, &fanSpeed));
    }
}

} // namespace ult
} // namespace L0
