/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/windows/os_fan_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/fan/windows/mock_fan.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class SysmanDeviceFanFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<FanKmdSysManager>> pKmdSysManager;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new Mock<FanKmdSysManager>);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pFanHandleContext->handleList.clear();
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_fan_handle_t> getFanHandles() {
        uint32_t count = 0;
        EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
        std::vector<zes_fan_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFansThenValidCountIsReturned) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, pKmdSysManager->mockSupportedFanCount);
}

TEST_F(SysmanDeviceFanFixture, GivenInvalidComponentCountWhenEnumeratingFansThenValidCountIsReturned) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, pKmdSysManager->mockSupportedFanCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, pKmdSysManager->mockSupportedFanCount);
}

TEST_F(SysmanDeviceFanFixture, GivenComponentCountZeroWhenEnumeratingFansThenValidFanHandlesAreReturned) {
    init(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, pKmdSysManager->mockSupportedFanCount);

    std::vector<zes_fan_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenNoFanSupportWhenEnumeratingFansThenZeroCountIsReturned) {
    init(false);

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::NumFanDomains] = 1;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::NumFanDomains] = 0;
}

TEST_F(SysmanDeviceFanFixture, GivenSingleSupportedCapabilityDisabledWhenEnumeratingFansThenCountIsReducedToOne) {
    pKmdSysManager.reset(new Mock<FanKmdSysManager>);
    pKmdSysManager->allowSetCalls = true;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x00;
    pKmdSysManager->mockSupportedFanCount = 3;

    pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
    pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();
    pSysmanDeviceImp->pFanHandleContext->handleList.clear();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFanFixture, GivenSingleFanWhenEnumeratingFansThenSingleCountIsReturned) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 1;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x02;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFanFixture, GivenMultipleFansWithSingleSupportWhenEnumeratingFansThenMultipleCountIsReturned) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 3;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x02;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 3u);
}

TEST_F(SysmanDeviceFanFixture, GivenMultipleFansWithoutSingleSupportWhenEnumeratingFansThenCountIsReducedToOne) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 3;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x01;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);
}

TEST_F(SysmanDeviceFanFixture, GivenFanModeCapabilitiesRequestFailureWhenEnumeratingFansThenOriginalCountIsReturned) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 2;
    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::SupportedFanModeCapabilities] = 1;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFans(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 2u);

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::SupportedFanModeCapabilities] = 0;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesThenCallSucceeds) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;

        ze_result_t result = zesFanGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxPoints), pKmdSysManager->mockFanMaxPoints);
        EXPECT_EQ(properties.maxRPM, static_cast<int32_t>(pKmdSysManager->mockMaxFanSpeed * 60));
        EXPECT_EQ(properties.supportedModes, zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(properties.supportedUnits, zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMaxFanSpeedRequestFailsWhenGettingFanPropertiesThenMaxRPMIsInvalid) {
    init(true);

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanSpeedSupported] = 1;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;

        ze_result_t result = zesFanGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(properties.canControl);
        EXPECT_EQ(static_cast<uint32_t>(properties.maxPoints), pKmdSysManager->mockFanMaxPoints);
        EXPECT_EQ(properties.maxRPM, -1);
        EXPECT_EQ(properties.supportedModes, zes_fan_speed_mode_t::ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(properties.supportedUnits, zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT);
    }

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanSpeedSupported] = 0;
}

TEST_F(SysmanDeviceFanFixture, GivenSetCallsDisabledWhenGettingFanPropertiesThenControlIsDisabled) {
    init(false);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties = {};
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMultipleFansWhenGettingFanPropertiesThenEachFanIsAccessedCorrectly) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 2;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x02;

    auto handles = getFanHandles();
    EXPECT_EQ(handles.size(), 2u);

    for (auto handle : handles) {
        zes_fan_properties_t properties;
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_TRUE(properties.canControl);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRequestMultipleFailsWhenGettingFanPropertiesThenErrorIsReturned) {
    init(true);

    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }

    pKmdSysManager->mockRequestMultiple = false;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanPropertiesThenResponseSizeIsValidated) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_properties_t properties;
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenResponseSizeMismatchWhenGettingFanPropertiesThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        zes_fan_properties_t properties;
        ze_result_t result = zesFanGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->requestMultipleSizeDiff = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenNoStockTableWhenGettingFanConfigInDefaultModeThenConfigHasZeroPoints) {
    init(true);

    auto handles = getFanHandles();

    pKmdSysManager->mockReturnNoStockTable = true;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);
    }

    pKmdSysManager->mockReturnNoStockTable = false;
}

TEST_F(SysmanDeviceFanFixture, GivenStockTableAvailableWhenGettingFanConfigInDefaultModeThenConfigHasValidPoints) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 10);
        EXPECT_EQ(fanConfig.speedTable.table[0].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[1].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[2].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[3].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMaxPointsRequestFailsWhenGettingFanConfigThenConfigHasZeroPoints) {
    init(true);

    auto handles = getFanHandles();

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 1;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);
    }

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 0;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWithZeroMaxPointsTableWhenGettingFanConfigThenSuccessIsReturnedWithoutDefaultFanTable) {
    // Setting allow set calls or not
    init(true);

    auto handles = getFanHandles();

    // Configure test scenario: force zero max points (no stock table)
    pKmdSysManager->mockReturnNoStockTable = true;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);
    }

    // Reset to default values
    pKmdSysManager->mockReturnNoStockTable = false;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWithSmallStockTableAvailableWhenGettingFanConfigThenSuccessIsReturnedWithDefaultFanTable) {
    // Setting allow set calls or not
    init(true);

    auto handles = getFanHandles();

    // Configure test scenario: small stock table available
    pKmdSysManager->mockReturnSmallStockTable = true;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 5);
    }

    // Reset to default values
    pKmdSysManager->mockReturnSmallStockTable = false;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigThenFirstSingleRequestFails) {
    // Setting allow set calls or not
    init(true);

    auto handles = getFanHandles();
    pKmdSysManager->mockRequestMultiple = true;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesFanGetConfig(handle, &fanConfig));
    }

    // Reset to default values
    pKmdSysManager->mockRequestMultiple = false;
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenGettingFanConfigWithValidFanPointsSuccessCustomFanTable) {
    // Setting allow set calls or not
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 4;
        fanSpeedTable.table[0].speed.speed = 65;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 75;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 45;

        fanSpeedTable.table[2].speed.speed = 85;
        fanSpeedTable.table[2].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[2].temperature = 60;

        fanSpeedTable.table[3].speed.speed = 100;
        fanSpeedTable.table[3].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[3].temperature = 90;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetSpeedTableMode(handle, &fanSpeedTable));

        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 4);
        EXPECT_EQ(fanConfig.speedTable.table[0].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[1].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[2].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
        EXPECT_EQ(fanConfig.speedTable.table[3].speed.units, ZES_FAN_SPEED_UNITS_PERCENT);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenPartialStockTableReadFailureWhenGettingFanConfigThenPartialTableIsReturned) {
    init(true);

    auto handles = getFanHandles();

    // Configure test scenario: some stock table points fail to read
    pKmdSysManager->mockReturnSmallStockTable = true;

    for (auto handle : handles) {
        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 5); // Should get partial table
    }

    // Reset
    pKmdSysManager->mockReturnSmallStockTable = false;
}

TEST_F(SysmanDeviceFanFixture, GivenTableModeWithCustomPointsWhenGettingFanConfigThenCustomTableIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 55;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 35;

        fanSpeedTable.table[1].speed.speed = 85;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 55;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetSpeedTableMode(handle, &fanSpeedTable));

        zes_fan_config_t fanConfig;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handle, &fanConfig));
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_TABLE);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 2);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenCurrentFanPointRequestFailsWhenGettingFanConfigThenSuccessIsReturnedWithFallbackBehavior) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 3;
        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentFanPoint] = 1;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_TABLE);

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentFanPoint] = 0;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenDefaultModeLoopRequestFailsWhenGettingFanConfigThenPartialTableIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 0;
        pKmdSysManager->mockReturnSmallStockTable = true;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockReturnSmallStockTable = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenDefaultModeLoopResponseSizeMismatchWhenGettingFanConfigThenPartialTableIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 0;
        pKmdSysManager->mockReturnSmallStockTable = true;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleSizeDiff = false;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockReturnSmallStockTable = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenDefaultModeMaxPointsResponseSizeMismatchWhenGettingFanConfigThenFallbackBehaviorIsUsed) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 0;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleSizeDiff = false;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenDefaultModeMaxPointsRequestFailsWhenGettingFanConfigThenFallbackBehaviorIsUsed) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 0;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidFanHandleWhenSettingDefaultModeThenCallSucceeds) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMultipleFansWhenSettingDefaultModeThenEachFanIsConfigured) {
    init(true);

    pKmdSysManager->mockSupportedFanCount = 2;
    pKmdSysManager->mockSupportedFanModeCapabilities = 0x02;

    auto handles = getFanHandles();
    EXPECT_EQ(handles.size(), 2u);

    for (auto handle : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handle));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRequestFailsWhenSettingDefaultModeThenErrorIsReturned) {
    init(true);

    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        ze_result_t result = zesFanSetDefaultMode(handle);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }

    pKmdSysManager->mockRequestMultiple = false;
}

TEST_F(SysmanDeviceFanFixture, GivenMaxControlPointsRequestFailsWhenSettingSpeedTableModeThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 50;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 80;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 60;

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 1;

        ze_result_t result = zesFanSetSpeedTableMode(handle, &fanSpeedTable);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenFixedSpeedModeWhenSettingFixedSpeedModeThenUnsupportedIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_t fanSpeed = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanSetFixedSpeedMode(handle, &fanSpeed));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenZeroControlPointsWhenSettingSpeedTableModeThenInvalidArgumentIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenTooManyControlPointsWhenSettingSpeedTableModeThenInvalidArgumentIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 20;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRPMUnitsWhenSettingSpeedTableModeThenInvalidArgumentIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 1500;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_RPM;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 2500;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 60;

        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMaxControlPointsRequestFailsWhenSettingSpeedTableModeThenDefaultMaxPointsAreUsed) {
    init(true);

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 1;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 65;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 75;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 45;

        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }

    pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 0;
}

TEST_F(SysmanDeviceFanFixture, GivenOddNumberOfControlPointsWhenSettingSpeedTableModeThenCallSucceeds) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 3;
        fanSpeedTable.table[0].speed.speed = 50;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 25;

        fanSpeedTable.table[1].speed.speed = 70;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 45;

        fanSpeedTable.table[2].speed.speed = 90;
        fanSpeedTable.table[2].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[2].temperature = 65;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetSpeedTableMode(handle, &fanSpeedTable));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenResponseSizeMismatchWhenSettingSpeedTableModeThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 65;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 75;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 45;

        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 1;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;

        ze_result_t result = zesFanSetSpeedTableMode(handle, &fanSpeedTable);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleSizeDiff = false;
        pKmdSysManager->requestMultipleCallCount = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenSetSpeedTableRequestFailsWhenSettingSpeedTableModeThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 50;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 80;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 60;

        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 1;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        ze_result_t result = zesFanSetSpeedTableMode(handle, &fanSpeedTable);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRPMUnitsWhenGettingFanSpeedThenValidSpeedIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_GT(fanSpeed, 0);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenPercentUnitsWhenGettingFanSpeedThenUnsupportedIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_PERCENT;
        int32_t fanSpeed = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFanGetState(handle, unit, &fanSpeed));
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRequestFailsWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }

    pKmdSysManager->mockRequestMultiple = false;
}

TEST_F(SysmanDeviceFanFixture, GivenCurrentFanSpeedRequestFailsWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
    }

    pKmdSysManager->mockRequestMultiple = false;
}

TEST_F(SysmanDeviceFanFixture, GivenRPMUnitsWhenGettingFanStateThenValidSpeedIsReturned) {
    init(true);

    pKmdSysManager->mockFanCurrentPulses = 42000;

    auto handles = getFanHandles();

    for (auto handle : handles) {
        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanSpeed, 42000);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenEmptyResponseWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();

    for (auto handle : handles) {
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);
        EXPECT_NE(ZE_RESULT_SUCCESS, result);

        pKmdSysManager->mockRequestMultiple = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRequestMultipleFailsWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        int32_t fanSpeed;

        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        ze_result_t result = zesFanGetState(handle, ZES_FAN_SPEED_UNITS_RPM, &fanSpeed);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

        pKmdSysManager->mockRequestMultiple = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenResponseSizeMismatchWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        int32_t fanSpeed;

        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;

        ze_result_t result = zesFanGetState(handle, ZES_FAN_SPEED_UNITS_RPM, &fanSpeed);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->requestMultipleSizeDiff = false;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenValidParametersWhenCreatingFanObjectThenObjectIsCreatedSuccessfully) {
    init(true);

    auto handles = getFanHandles();
    EXPECT_GT(handles.size(), 0u);

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFanFixture, GivenMaxControlPointsResponseFailsWhenSettingSpeedTableModeThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 50;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 80;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 60;

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 1;

        ze_result_t result = zesFanSetSpeedTableMode(handle, &fanSpeedTable);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::MaxFanControlPointsSupported] = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenCurrentFanSpeedResponseFailsWhenGettingFanStateThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentFanSpeed] = 1;

        zes_fan_speed_units_t unit = zes_fan_speed_units_t::ZES_FAN_SPEED_UNITS_RPM;
        int32_t fanSpeed = 0;
        ze_result_t result = zesFanGetState(handle, unit, &fanSpeed);

        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentFanSpeed] = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenDefaultModeRequestFailsWhenGettingFanConfigThenFallbackIsUsed) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 0;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);
        EXPECT_EQ(fanConfig.speedTable.numPoints, 0);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenRequestMultipleFailsWhenSettingSpeedTableModeThenErrorIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        zes_fan_speed_table_t fanSpeedTable = {0};
        fanSpeedTable.numPoints = 2;
        fanSpeedTable.table[0].speed.speed = 50;
        fanSpeedTable.table[0].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[0].temperature = 30;

        fanSpeedTable.table[1].speed.speed = 80;
        fanSpeedTable.table[1].speed.units = ZES_FAN_SPEED_UNITS_PERCENT;
        fanSpeedTable.table[1].temperature = 60;

        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 1;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

        ze_result_t result = zesFanSetSpeedTableMode(handle, &fanSpeedTable);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenCurrentControlPointsResponseFailsWhenGettingFanConfigThenDefaultConfigIsReturned) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentNumOfControlPoints] = 1;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(fanConfig.mode, ZES_FAN_SPEED_MODE_DEFAULT);

        pKmdSysManager->mockFanFailure[KmdSysman::Requests::Fans::CurrentNumOfControlPoints] = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenTableModeRequestFailsWhenGettingFanConfigThenFirstConditionCoverageIsAchieved) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 3;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNKNOWN;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

TEST_F(SysmanDeviceFanFixture, GivenTableModeResponseSizeMismatchWhenGettingFanConfigThenSecondConditionCoverageIsAchieved) {
    init(true);

    auto handles = getFanHandles();
    for (auto handle : handles) {
        pKmdSysManager->mockFanCurrentFanPoints = 3;
        pKmdSysManager->mockRequestMultiple = true;
        pKmdSysManager->failSelectiveRequestMultipleCount = 3;
        pKmdSysManager->requestMultipleSizeDiff = true;
        pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;

        zes_fan_config_t fanConfig;
        ze_result_t result = zesFanGetConfig(handle, &fanConfig);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        pKmdSysManager->mockRequestMultiple = false;
        pKmdSysManager->failSelectiveRequestMultipleCount = 0;
        pKmdSysManager->requestMultipleSizeDiff = false;
        pKmdSysManager->requestMultipleCallCount = 0;
        pKmdSysManager->mockFanCurrentFanPoints = 0;
    }
}

} // namespace ult
} // namespace L0