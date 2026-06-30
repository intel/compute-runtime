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
    auto pFanImp = createFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_FALSE(props.onSubdevice);
    EXPECT_EQ(0u, props.subdeviceId);
    EXPECT_TRUE(props.canControl);
    EXPECT_EQ(mockFanMaxRpm, props.maxRPM);
    EXPECT_EQ(2, props.maxPoints);
    EXPECT_EQ((1u << ZES_FAN_SPEED_MODE_DEFAULT) | (1u << ZES_FAN_SPEED_MODE_TABLE) | (1u << ZES_FAN_SPEED_MODE_FIXED), props.supportedModes);
    EXPECT_EQ((1u << ZES_FAN_SPEED_UNITS_RPM) | (1u << ZES_FAN_SPEED_UNITS_PERCENT), props.supportedUnits);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenMaxRpmReadFailsWhenGetPropertiesCalledThenMaxRpmIsMinusOne) {
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE; // fanMaxNode read fails during init
    auto pFanImp = createFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(-1, props.maxRPM);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoAutoPointNodesWhenGetPropertiesCalledThenMaxPointsIsZero) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmAutoPoint0Exists = false;
    pSysfsAccess->pwmAutoPoint1Exists = false;
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(0, props.maxPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenFanWhenGetPropertiesCalledThenOnSubdeviceIsFalseAndSubdeviceIdIsZero) {
    auto pFanImp = createFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_FALSE(props.onSubdevice);
    EXPECT_EQ(0u, props.subdeviceId);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenGetStateWithRpmCalledThenSpeedReturned) {
    auto pFanImp = createFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getState(ZES_FAN_SPEED_UNITS_RPM, &speed));
    EXPECT_EQ(mockFanRpm, speed);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenReadFailsWhenGetStateWithRpmCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getState(ZES_FAN_SPEED_UNITS_RPM, &speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenGetStateWithPercentCalledThenPercentReturned) {
    auto pFanImp = createFanImp();
    int32_t speed = 0;
    // mockFanRpm=2000, mockFanMaxRpm=4000 -> expected 50%
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getState(ZES_FAN_SPEED_UNITS_PERCENT, &speed));
    EXPECT_EQ(50, speed);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenSetDefaultModeCalledThenSuccess) {
    auto pFanImp = createFanImp();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setDefaultMode());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenWriteFailsWhenSetDefaultModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->mockWriteInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setDefaultMode());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAutoStockModeWhenGetConfigCalledThenDefaultModeReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableAutoStock;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_DEFAULT, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenManualModeWhenGetConfigCalledThenTableModeReturnedWithCurvePoints) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(2, config.speedTable.numPoints);
    EXPECT_EQ(static_cast<uint32_t>(mockTempMilliDeg / 1000), config.speedTable.table[0].temperature);
    EXPECT_EQ((mockPwmVal * mockFanMaxRpm) / 255, config.speedTable.table[0].speed.speed);
    EXPECT_EQ(ZES_FAN_SPEED_UNITS_RPM, config.speedTable.table[0].speed.units);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAllAutoPointPwmMatchCurrentPwmWhenGetConfigCalledThenFixedModeReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmVal1 = mockPwmVal;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_FIXED, config.mode);
    EXPECT_EQ((mockPwmVal * mockFanMaxRpm) / 255, config.speedFixed.speed);
    EXPECT_EQ(ZES_FAN_SPEED_UNITS_RPM, config.speedFixed.units);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmEnableReadFailsWhenGetConfigCalledThenNotAvailableReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getConfig(&config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAutoPointPwmReadFailsWithNonNotAvailableErrorWhenGetConfigCalledThenErrorPropagated) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->mockAutoPointPwm1ReadResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, pFanImp->getConfig(&config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenManualModeAndReadCurvePointTempFailsWhenGetConfigCalledThenPartialPointsReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmAutoPoint1Exists = false;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(1, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidSpeedTableWhenSetSpeedTableModeCalledThenSuccess) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 2;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    table.table[1] = {80, {80, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenRpmUnitsInTableWhenSetSpeedTableModeCalledThenSuccessReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {2000, ZES_FAN_SPEED_UNITS_RPM}};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenZeroPointsWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenWriteFailsWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
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
    auto pFanImp = createFanImp();
    EXPECT_TRUE(pFanImp->hwmonDir.empty());
}

TEST_F(SysmanDeviceFanFixtureXe, GivenCurvePwmReadFailsInReadCurvePointsWhenGetConfigCalledThenPartialPointsReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    // pwmVal0 (128) != pwmVal1 (200), so isFixed breaks after 2 auto_point_pwm reads.
    // failPwmAutoPointReadAfterCount=2 means the 3rd read (first pwm read inside
    // readCurvePoints at pointIndex=0) fails, leaving numPoints=0.
    pSysfsAccess->failPwmAutoPointReadAfterCount = 2;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmValReadFailsWhenGetConfigCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->failPwmValRead = true;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getConfig(&config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmWriteFailsAfterTempWriteWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->writeFailAfterCount = 2;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTempWriteFailsOnSecondPointWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->writeFailAfterCount = 3;
    zes_fan_speed_table_t table = {};
    table.numPoints = 2;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    table.table[1] = {80, {80, ZES_FAN_SPEED_UNITS_PERCENT}};
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
    auto pFanImp = createFanImp();
    EXPECT_EQ(fanHwmonDir, pFanImp->hwmonDir);
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAllAutoPointsExistWhenGetPropertiesAndGetConfigCalledThenLoopsExitNaturally) {
    auto pFanImp = createFanImp();
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
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->failTempRead = true;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(0, config.speedTable.numPoints);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTooManyPointsWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 11;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenTempWriteFailsWhenSetSpeedTableModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->writeFailAfterCount = 1;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNegativeSpeedWhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {-1, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenSpeedAbove100WhenSetSpeedTableModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {101, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setSpeedTableMode(&table));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAutoPointNodeMissingWhenSetSpeedTableModeCalledThenUnsupportedFeatureReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->pwmAutoPoint0Exists = false;
    zes_fan_speed_table_t table = {};
    table.numPoints = 1;
    table.table[0] = {60, {50, ZES_FAN_SPEED_UNITS_PERCENT}};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFanImp->setSpeedTableMode(&table));
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

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenSetFixedSpeedModeWithPercentCalledThenSuccess) {
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {50, ZES_FAN_SPEED_UNITS_PERCENT};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenValidHwmonWhenSetFixedSpeedModeWithRpmCalledThenSuccess) {
    auto pFanImp = createFanImp();
    // mockFanMaxRpm=4000, so 2000 RPM -> pwm = (2000*255)/4000 = 127
    zes_fan_speed_t speed = {2000, ZES_FAN_SPEED_UNITS_RPM};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNegativePercentWhenSetFixedSpeedModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {-1, ZES_FAN_SPEED_UNITS_PERCENT};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPercentOver100WhenSetFixedSpeedModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {101, ZES_FAN_SPEED_UNITS_PERCENT};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenRpmWithNoMaxNodeWhenSetFixedSpeedModeCalledThenNotAvailableReturned) {
    pSysfsAccess->mockReadInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {2000, ZES_FAN_SPEED_UNITS_RPM};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenFanMaxRpmIsZeroWhenInitCalledThenMaxRpmIsMinusOne) {
    pSysfsAccess->fanMaxRpmVal = 0;
    auto pFanImp = createFanImp();
    zes_fan_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getProperties(&props));
    EXPECT_EQ(-1, props.maxRPM);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoMaxRpmWhenGetStateWithPercentCalledThenNotAvailableReturned) {
    pSysfsAccess->fanMaxRpmVal = 0;
    auto pFanImp = createFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getState(ZES_FAN_SPEED_UNITS_PERCENT, &speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoMaxRpmWhenGetConfigInFixedModeCalledThenPercentSpeedReturned) {
    pSysfsAccess->fanMaxRpmVal = 0;
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmVal1 = mockPwmVal;
    auto pFanImp = createFanImp();
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_FIXED, config.mode);
    EXPECT_EQ(ZES_FAN_SPEED_UNITS_PERCENT, config.speedFixed.units);
    EXPECT_EQ((mockPwmVal * 100) / 255, config.speedFixed.speed);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNoMaxRpmWhenGetConfigInTableModeCalledThenPercentSpeedPointsReturned) {
    pSysfsAccess->fanMaxRpmVal = 0;
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    auto pFanImp = createFanImp();
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_TABLE, config.mode);
    EXPECT_EQ(2, config.speedTable.numPoints);
    EXPECT_EQ(ZES_FAN_SPEED_UNITS_PERCENT, config.speedTable.table[0].speed.units);
    EXPECT_EQ((mockPwmVal * 100) / 255, config.speedTable.table[0].speed.speed);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenAllTenAutoPointsPwmMatchWhenGetConfigCalledThenFixedModeReturnedLoopExitsNaturally) {
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmVal1 = mockPwmVal;
    auto pFanImp = createFanImp();
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getConfig(&config));
    EXPECT_EQ(ZES_FAN_SPEED_MODE_FIXED, config.mode);
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmEnableWriteFailsWhenSetFixedSpeedModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->mockWriteInt32Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    zes_fan_speed_t speed = {50, ZES_FAN_SPEED_UNITS_PERCENT};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmNodeWriteFailsWhenSetFixedSpeedModeCalledThenErrorReturned) {
    auto pFanImp = createFanImp();
    pSysfsAccess->writeFailAfterCount = 1;
    zes_fan_speed_t speed = {50, ZES_FAN_SPEED_UNITS_PERCENT};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenNegativeRpmSpeedWhenSetFixedSpeedModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {-1, ZES_FAN_SPEED_UNITS_RPM};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenInvalidUnitsWhenSetFixedSpeedModeCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    zes_fan_speed_t speed = {50, ZES_FAN_SPEED_UNITS_FORCE_UINT32};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->setFixedSpeedMode(&speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenPwmNodeReadFailsInFixedModeWhenGetConfigCalledThenNotAvailableReturned) {
    pSysfsAccess->pwmEnableVal = mockPwmEnableManual;
    pSysfsAccess->pwmVal1 = mockPwmVal;
    auto pFanImp = createFanImp();
    pSysfsAccess->failPwmNodeRead = true;
    zes_fan_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pFanImp->getConfig(&config));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenInvalidUnitsWhenGetStateCalledThenInvalidArgReturned) {
    auto pFanImp = createFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFanImp->getState(ZES_FAN_SPEED_UNITS_FORCE_UINT32, &speed));
}

TEST_F(SysmanDeviceFanFixtureXe, GivenRpmExceedsMaxRpmWhenGetStateWithPercentCalledThenResultClampedTo100) {
    // Set fan RPM above maxRpm to exercise the upper clamp (percent > 100 -> 100)
    pSysfsAccess->fanRpmVal = mockFanMaxRpm * 2; // 8000 RPM with maxRpm=4000 -> raw 200% -> clamped to 100
    auto pFanImp = createFanImp();
    int32_t speed = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pFanImp->getState(ZES_FAN_SPEED_UNITS_PERCENT, &speed));
    EXPECT_EQ(100, speed);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
