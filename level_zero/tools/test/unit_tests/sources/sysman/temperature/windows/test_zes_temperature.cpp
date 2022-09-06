/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/windows/os_temperature_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/temperature/windows/mock_temperature.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t temperatureHandleComponentCount = 3u;
class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {

  protected:
    Mock<TemperatureKmdSysManager> *pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pKmdSysManager = new Mock<TemperatureKmdSysManager>;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager;

        for (auto handle : pSysmanDeviceImp->pTempHandleContext->handleList) {
            delete handle;
        }

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
        getTempHandles(0);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenInvalidComponentCountWhenEnumeratingTemperatureSensorsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenComponentCountZeroWhenEnumeratingTemperatureSensorsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, temperatureHandleComponentCount);

    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidPowerHandleWhenGettingTemperaturePropertiesAllowSetToTrueThenCallSucceeds) {
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

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenGettingMemoryTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_MEMORY], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempMemory));
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GPU], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGPU));
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handles[ZES_TEMP_SENSORS_GLOBAL], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(pKmdSysManager->mockTempGlobal));
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    auto pTemperatureImpMemory = std::make_unique<TemperatureImp>(deviceHandles[0], pOsSysman, ZES_TEMP_SENSORS_GLOBAL_MIN);
    auto pWddmTemperatureImp = static_cast<WddmTemperatureImp *>(pTemperatureImpMemory->pOsTemperature.get());
    double pTemperature = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmTemperatureImp->getSensorTemperature(&pTemperature));
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

TEST_F(SysmanDeviceTemperatureFixture, DISABLED_GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

} // namespace ult
} // namespace L0
