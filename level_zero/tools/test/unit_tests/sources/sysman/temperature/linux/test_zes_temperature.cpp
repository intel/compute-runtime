/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/temperature/linux/mock_sysfs_temperature.h"

namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 3u;
const std::string deviceName("testDevice");
FsAccess *pFsAccessTemp = nullptr;
class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<TemperaturePmt>> pPmt;
    PlatformMonitoringTech *pPmtOld = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pPmtOld = pLinuxSysmanImp->pPmt;
        pPmt = std::make_unique<NiceMock<Mock<TemperaturePmt>>>();
        pPmt->setVal(true);
        pPmt->init(deviceName, pFsAccessTemp);
        pLinuxSysmanImp->pPmt = pPmt.get();
        pSysmanDeviceImp->pTempHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pPmt = pPmtOld;
    }

    std::vector<zes_temp_handle_t> get_temp_handles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, handleComponentCount);

    count = 0;
    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = get_temp_handles(handleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[1], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(tempArr[computeIndex]));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = get_temp_handles(handleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[0], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(tempArr[globalIndex]));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    auto pTemperatureImpMemory = std::make_unique<TemperatureImp>(pOsSysman, ZES_TEMP_SENSORS_GLOBAL_MIN);
    auto pLinuxTemperatureImpMemory = static_cast<LinuxTemperatureImp *>(pTemperatureImpMemory->pOsTemperature);
    double pTemperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxTemperatureImpMemory->getSensorTemperature(&pTemperature));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenPmtSupportNotAvailableWhenCheckingForTempModuleSupportThenTempModuleSupportShouldReturnFalse) {
    //delete previously allocated memory, allocated by init() call in SetUp()
    if (pPmt->mappedMemory != nullptr) {
        delete pPmt->mappedMemory;
        pPmt->mappedMemory = nullptr;
    }
    pPmt->setVal(false);
    //FsAccess *pFsAccessTemp = nullptr;
    pPmt->init(deviceName, pFsAccessTemp);
    auto handles = get_temp_handles(handleComponentCount);
    for (auto handle : handles) {
        auto pTemperatureImpTest = static_cast<TemperatureImp *>(L0::Temperature::fromHandle(handle));
        EXPECT_FALSE(pTemperatureImpTest->pOsTemperature->isTempModuleSupported());
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenPmtSupportAvailableWhenCheckingForTempModuleSupportThenTempModuleSupportShouldReturnTrue) {
    //delete previously allocated memory, allocated by init() call in SetUp()
    if (pPmt->mappedMemory != nullptr) {
        delete pPmt->mappedMemory;
        pPmt->mappedMemory = nullptr;
    }
    pPmt->setVal(true);
    //FsAccess *pFsAccessTemp = nullptr;
    pPmt->init(deviceName, pFsAccessTemp);
    auto handles = get_temp_handles(handleComponentCount);
    for (auto handle : handles) {
        auto pTemperatureImpTest = static_cast<TemperatureImp *>(L0::Temperature::fromHandle(handle));
        EXPECT_TRUE(pTemperatureImpTest->pOsTemperature->isTempModuleSupported());
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperaturePropertiesThenUnsupportedIsReturned) {
    auto handles = get_temp_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = get_temp_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = get_temp_handles(handleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

} // namespace ult
} // namespace L0
