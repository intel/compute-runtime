/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/fan/linux/mock_fan.h"
#include "level_zero/sysman/test/unit_tests/sources/fan/linux/mock_sysfs_fan_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanDeviceFanFixtureXe, GivenXeHwmonPresentWhenEnumeratingFansThenHandlesAreReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(1u, count);

    std::vector<zes_fan_handle_t> handles(count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, handles.data()));
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFanFixtureXe, GivenHwmonNameDoesNotMatchWhenEnumeratingFansThenNoHandlesReturned) {
    pSysfsAccess->mockReadStringResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenScanDirFailsWhenEnumeratingFansThenNoHandlesReturned) {
    pSysfsAccess->mockScanDirEntriesResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenHwmonDirScanFailsWhenEnumeratingFansThenZeroFansReturned) {
    pSysfsAccess->failHwmonDirScan = true;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoFanInputNodeWhenEnumeratingFansThenZeroFansReturned) {
    pSysfsAccess->fanCount = 0;

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(0u, count);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenSubDeviceCountNonZeroWhenInitFanContextThenSubDeviceHandlesCreated) {
    auto fanContext = std::make_unique<MockFanHandleContext>(pOsSysman);
    fanContext->mockSupportedFanCount = 0;
    fanContext->init();
    EXPECT_EQ(0u, fanContext->handleList.size());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenGetPropertiesCalledThenSuccessAndCorrectValuesReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_FALSE(props.onSubdevice);
    EXPECT_EQ(0u, props.subdeviceId);
    EXPECT_TRUE(props.canControl);
    EXPECT_EQ(mockFanMaxRpm, props.maxRPM);
    EXPECT_EQ(2, props.maxPoints);
    EXPECT_EQ((1u << ZES_FAN_SPEED_MODE_DEFAULT) | (1u << ZES_FAN_SPEED_MODE_TABLE), props.supportedModes);
    EXPECT_EQ(1u << ZES_FAN_SPEED_UNITS_RPM, props.supportedUnits);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenMaxRpmReadFailsWhenGetPropertiesCalledThenMaxRpmIsMinusOne) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->fanMaxExists = false;
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(-1, props.maxRPM);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoAutoPointNodesWhenGetPropertiesCalledThenMaxPointsIsZero) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmAutoPoint0Exists = false;
    pSysfsAccess->pwmAutoPoint1Exists = false;
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(0, props.maxPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenFanWhenGetPropertiesCalledThenOnSubdeviceIsFalseAndSubdeviceIdIsZero) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_FALSE(props.onSubdevice);
    EXPECT_EQ(0u, props.subdeviceId);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenGetStateWithRpmCalledThenSpeedReturned) {
    auto pFanImp = makeLinuxFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getState(ZES_FAN_SPEED_UNITS_RPM, &speed));
    EXPECT_EQ(mockFanRpm, speed);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenReadFailsWhenGetStateWithRpmCalledThenErrorReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getState(ZES_FAN_SPEED_UNITS_RPM, &speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenGetStateWithPercentCalledThenUnsupportedReturned) {
    auto pFanImp = makeLinuxFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFanImp->getState(ZES_FAN_SPEED_UNITS_PERCENT, &speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenSetDefaultModeCalledThenSuccess) {
    auto pFanImp = makeLinuxFanImp();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setDefaultMode());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenWriteFailsWhenSetDefaultModeCalledThenErrorReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->mockWriteInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setDefaultMode());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAutoStockModeWhenGetConfigCalledThenDefaultModeReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableAutoStock;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_DEFAULT, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenFullSpeedModeWhenGetConfigCalledThenDefaultModeReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableFullSpeed;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_DEFAULT, config.mode);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenManualModeWhenGetConfigCalledThenTableModeReturnedWithCurvePoints) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(2, config.speedTable.numPoints);
    EXPECT_EQ(static_cast<uint32_t>(mockTempMilliDeg / 1000), config.speedTable.table[0].temperature);
    EXPECT_EQ((mockPwmVal * 100) / 255, config.speedTable.table[0].speed.speed);
    EXPECT_EQ(ZES_FAN_SPEED_UNITS_PERCENT, config.speedTable.table[0].speed.units);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenManualModeAndNoAutoPointNodesWhenGetConfigCalledThenTableModeWithZeroPoints) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmAutoPoint0Exists = false;
    pSysfsAccess->pwmAutoPoint1Exists = false;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmEnableReadFailsWhenGetConfigCalledThenNotAvailableReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getConfig(&config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenManualModeAndReadCurvePointTempFailsWhenGetConfigCalledThenPartialPointsReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmAutoPoint1Exists = false;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(1, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidSpeedTableWhenSetSpeedTableModeCalledThenSuccess) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 2;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    table.table[1] = {80, {80, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenRpmUnitsInTableWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {2000, ZES_FAN_SPEED_UNITS_RPM}};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenZeroPointsWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenWriteFailsWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->mockWriteInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenKmdInterfaceWhenFanNodePathsRequestedThenCorrectPathsReturned) {
    const std::string dir = fanHwmonDir;
    EXPECT_EQ(dir + "/fan1_input", pSysmanKmdInterface->getFanInputNode(dir, 1));
    EXPECT_EQ(dir + "/fan1_max", pSysmanKmdInterface->getFanMaxNode(dir, 1));
    EXPECT_EQ(dir + "/pwm1", pSysmanKmdInterface->getPwmNode(dir, 1));
    EXPECT_EQ(dir + "/pwm1_enable", pSysmanKmdInterface->getPwmEnableNode(dir, 1));
    EXPECT_EQ(dir + "/pwm1_auto_point1_temp", pSysmanKmdInterface->getPwmAutoPointTempNode(dir, 1, 0));
    EXPECT_EQ(dir + "/pwm1_auto_point2_temp", pSysmanKmdInterface->getPwmAutoPointTempNode(dir, 1, 1));
    EXPECT_EQ(dir + "/pwm1_auto_point1_pwm", pSysmanKmdInterface->getPwmAutoPointPwmNode(dir, 1, 0));
    EXPECT_EQ(dir + "/pwm2_auto_point3_pwm", pSysmanKmdInterface->getPwmAutoPointPwmNode(dir, 2, 2));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenHwmonNameMismatchWhenInitCalledThenHwmonDirRemainsEmpty) {
    pSysfsAccess->returnWrongHwmonName = true;
    auto pFanImp = makeLinuxFanImp();
    EXPECT_TRUE(pFanImp->hwmonDir.empty());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmValReadFailsWhenGetConfigCalledThenPartialCurveReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->failPwmValRead = true;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmWriteFailsAfterTempWriteWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->writeFailAfterCount = 2;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenSubDeviceCountOneWhenFanContextInitCalledThenSubDeviceHandleCreated) {
    auto fanContext = std::make_unique<L0::Sysman::FanHandleContext>(pOsSysman);
    fanContext->init();
    EXPECT_EQ(1u, fanContext->handleList.size());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenI915KmdInterfaceWhenFanNodeMethodsCalledThenEmptyStringsReturned) {
    auto pI915KmdInterface = std::make_unique<MockSysmanKmdInterfaceUpstream>(pLinuxSysmanImp->getSysmanProductHelper());
    const std::string dir = "device/hwmon/hwmon0";
    EXPECT_EQ(std::string{}, pI915KmdInterface->getFanInputNode(dir, 1));
    EXPECT_EQ(std::string{}, pI915KmdInterface->getFanMaxNode(dir, 1));
    EXPECT_EQ(std::string{}, pI915KmdInterface->getPwmNode(dir, 1));
    EXPECT_EQ(std::string{}, pI915KmdInterface->getPwmEnableNode(dir, 1));
    EXPECT_EQ(std::string{}, pI915KmdInterface->getPwmAutoPointTempNode(dir, 1, 0));
    EXPECT_EQ(std::string{}, pI915KmdInterface->getPwmAutoPointPwmNode(dir, 1, 0));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenFirstHwmonNameReadFailsWhenInitCalledThenSecondEntryUsed) {
    pSysfsAccess->returnTwoEntriesFirstReadFails = true;
    auto pFanImp = makeLinuxFanImp();
    EXPECT_EQ(fanHwmonDir, pFanImp->hwmonDir);
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAllAutoPointsExistWhenGetPropertiesAndGetConfigCalledThenLoopsExitNaturally) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->allAutoPointsExist = true;
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(10, props.maxPoints);
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTempReadFailsWhenGetConfigCalledThenZeroPointsInTable) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->failTempRead = true;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTooManyPointsWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 11;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTempWriteFailsWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->writeFailAfterCount = 1;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNegativeSpeedWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {-1, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenSpeedAbove100WhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = makeLinuxFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {101, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAutoPointNodeMissingWhenSetSpeedTableModeCalledThenUnsupportedFeatureReturned) {
    auto pFanImp = makeLinuxFanImp();
    pSysfsAccess->pwmAutoPoint0Exists = false;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAllFansExistWhenEnumeratingFansThenThreeFansFound) {
    pSysfsAccess->fanCount = 3;
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    EXPECT_EQ(3u, count);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidFanHandleWhenGetPropertiesCalledViaZesApiThenSuccessReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);
    std::vector<zes_fan_handle_t> handles(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, handles.data()));

    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetProperties(handles[0], &props));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidFanHandleWhenGetConfigCalledViaZesApiThenSuccessReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);
    std::vector<zes_fan_handle_t> handles(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, handles.data()));

    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanGetConfig(handles[0], &config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidFanHandleWhenSetDefaultModeCalledViaZesApiThenSuccessReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);
    std::vector<zes_fan_handle_t> handles(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, handles.data()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetDefaultMode(handles[0]));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidFanHandleWhenSetSpeedTableModeCalledViaZesApiThenSuccessReturned) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, nullptr));
    ASSERT_EQ(1u, count);
    std::vector<zes_fan_handle_t> handles(count);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFans(device->toHandle(), &count, handles.data()));

    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFanSetSpeedTableMode(handles[0], &table));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
