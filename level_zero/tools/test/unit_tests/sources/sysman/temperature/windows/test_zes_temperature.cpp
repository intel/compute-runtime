/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/source/sysman/temperature/windows/os_temperature_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/temperature/windows/mock_temperature.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<TemperatureKmdSysManager>> pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new Mock<TemperatureKmdSysManager>);
        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pSysmanDeviceImp->pTempHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }

        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenTempDomainsAreEnumeratedWhenCallingIsTempInitCompletedThenVerifyTempInitializationIsCompleted) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    EXPECT_EQ(true, pSysmanDeviceImp->pTempHandleContext->isTempInitDone());
}

TEST_F(SysmanDeviceTemperatureFixture, GivenInvalidComponentCountWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidDeviceHandleWhenEnumeratingTemperatureSensorsOnIntegratedDeviceThenZeroCountIsReturned) {
    uint32_t count = 0;
    pKmdSysManager->isIntegrated = true;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidPowerHandleWhenGettingTemperaturePropertiesAllowSetToTrueThenCallSucceeds) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    uint32_t sensorTypeIndex = 0;
    for (auto handle : handles) {
        zes_temp_properties_t properties;

        ze_result_t result = zesTemperatureGetProperties(handle, &properties);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
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
    auto pTemperatureImpMemory = std::make_unique<TemperatureImp>(deviceHandles[0], pOsSysman, ZES_TEMP_SENSORS_GLOBAL_MIN);
    auto pWddmTemperatureImp = static_cast<WddmTemperatureImp *>(pTemperatureImpMemory->pOsTemperature.get());
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
} // namespace L0
