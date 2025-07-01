/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/device/sysman_os_device.h"
#include "level_zero/sysman/source/driver/sysman_os_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

TEST_F(SysmanDeviceFixture, GivenSysmanDeviceHandleWhenCallingSysmanDeviceFunctionswithSurvivabilityModeSetToTrueThenSurvivabiityModeDetectedErrorIsReturned) {

    uint32_t count = 0;
    pSysmanDevice->isDeviceInSurvivabilityMode = true;

    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::performanceGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::powerGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::powerGetCardDomain(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::frequencyGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fabricPortGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::temperatureGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::standbyGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetProperties(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetSubDeviceProperties(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::processesGetState(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetState(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceReset(pSysmanDevice, false));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::engineGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetProperties(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetState(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetBars(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetStats(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::schedulerGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::rasGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::memoryGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fanGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::diagnosticsGet(pSysmanDevice, &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEventRegister(pSysmanDevice, uint32_t(0)));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEccAvailable(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEccConfigurable(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetEccState(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceSetEccState(pSysmanDevice, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceResetExt(pSysmanDevice, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fabricPortGetMultiPortThroughput(pSysmanDevice, count, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEnumEnabledVF(pSysmanDevice, &count, nullptr));

    pSysmanDevice->isDeviceInSurvivabilityMode = false;
}

TEST_F(SysmanDeviceFixture, GivenValidSysmanDeviceHandleWhenQueryingSurvivabilityModeStatusThenFalseIsReturned) {

    auto hwDeviceId = std::make_unique<NEO::HwDeviceId>(NEO::DriverModelType::wddm);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pOsSysman->initSurvivabilityMode(std::move(hwDeviceId)));
    EXPECT_FALSE(pOsSysman->isDeviceInSurvivabilityMode());
}

struct SysmanDriverTestSurvivabilityDevice : public ::testing::Test {};

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenOsDriverHandleWhenInitializingSurvivabilityModeThenUninitialisedErrorIsReturned) {
    auto pOsDriver = OsDriver::create();
    auto hwdeviceIds = pOsDriver->discoverDevicesWithSurvivabilityMode();
    EXPECT_EQ(0u, static_cast<uint32_t>(hwdeviceIds.size()));
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    uint32_t driverCount = 0;

    SysmanDriverHandle *pSysmanDriverHandle = pOsDriver->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(nullptr, pSysmanDriverHandle);
    EXPECT_EQ(0u, driverCount);

    pOsDriver->initSurvivabilityDevices(pSysmanDriverHandle, &result);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);

    auto hwDeviceId = std::make_unique<NEO::HwDeviceId>(NEO::DriverModelType::wddm);
    auto pOsSysmanSurvivabilityDevice = OsSysmanSurvivabilityDevice::createSurvivabilityDevice(std::move(hwDeviceId));
    EXPECT_EQ(nullptr, pOsSysmanSurvivabilityDevice);
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSysmanDeviceImpWhenInitializingSurvivabilityModeThenUninitialisedErrorIsReturned) {
    SysmanDeviceImp *pSysmanDevice = new SysmanDeviceImp();
    EXPECT_NE(nullptr, pSysmanDevice);
    EXPECT_NE(nullptr, pSysmanDevice->pOsSysman);
    auto hwDeviceId = std::make_unique<NEO::HwDeviceId>(NEO::DriverModelType::wddm);
    ze_result_t result = pSysmanDevice->pOsSysman->initSurvivabilityMode(std::move(hwDeviceId));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    delete pSysmanDevice;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
