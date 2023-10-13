/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/temperature/windows/sysman_os_temperature_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/windows/mock_temperature.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<TemperatureKmdSysManager> pKmdSysManager = nullptr;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new TemperatureKmdSysManager);
        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pTempHandleContext->handleList.clear();
    }
    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenTempDomainsAreEnumeratedWhenCallingIsTempInitCompletedThenVerifyTempInitializationIsCompleted) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    EXPECT_EQ(true, pSysmanDeviceImp->pTempHandleContext->isTempInitDone());
}

TEST_F(SysmanDeviceTemperatureFixture, GivenInvalidComponentCountWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidDeviceHandleWhenEnumeratingTemperatureSensorsOnIntegratedDeviceThenZeroCountIsReturned) {
    uint32_t count = 0;
    pKmdSysManager->isIntegrated = true;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidPowerHandleWhenGettingTemperaturePropertiesAllowSetToTrueThenCallSucceeds) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    uint32_t sensorTypeIndex = 0;
    for (auto handle : handles) {
        zes_temp_properties_t properties;

        ze_result_t result = zesTemperatureGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0);
        EXPECT_FALSE(properties.isCriticalTempSupported);
        EXPECT_FALSE(properties.isThreshold1Supported);
        EXPECT_FALSE(properties.isThreshold2Supported);
        EXPECT_EQ(properties.maxTemperature, pKmdSysManager->mockMaxTemperature);
        EXPECT_EQ(properties.type, pKmdSysManager->mockSensorTypes[sensorTypeIndex++]);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingMemoryTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_MEMORY], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempMemory));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GPU], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGPU));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGlobal));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    auto pTemperatureImpMemory = std::make_unique<L0::Sysman::TemperatureImp>(pOsSysman, false, 0, ZES_TEMP_SENSORS_GLOBAL_MIN);
    auto pWddmTemperatureImp = static_cast<L0::Sysman::WddmTemperatureImp *>(pTemperatureImpMemory->pOsTemperature.get());
    double pTemperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&pTemperature));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
