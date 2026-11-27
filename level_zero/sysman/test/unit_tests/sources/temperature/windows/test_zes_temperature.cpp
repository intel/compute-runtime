/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/windows/sysman_os_temperature_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/windows/mock_temperature.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"
#include "level_zero/zes_intel_gpu_sysman.h"

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

        auto pPmt = new PublicPlatformMonitoringTech(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
        pWddmSysmanImp->pPmt.reset(pPmt);

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

TEST_F(SysmanDeviceTemperatureFixture, GivenValidOsTemperatureObjectWhenGettingTemperaturePropertiesThenCallSucceeds) {
    std::unique_ptr<PublicWddmTemperatureImp> pWddmTemperatureImp = std::make_unique<PublicWddmTemperatureImp>(pOsSysman);
    zes_temp_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pWddmTemperatureImp->getProperties(&properties));
    EXPECT_FALSE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_FALSE(properties.isCriticalTempSupported);
    EXPECT_FALSE(properties.isThreshold1Supported);
    EXPECT_FALSE(properties.isThreshold2Supported);
    EXPECT_EQ(properties.maxTemperature, pKmdSysManager->mockMaxTemperature);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingCriticalPropertiesThenZeroCriticalTemperatureIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_intel_temp_critical_exp_properties_t criticalProperties = {ZES_INTEL_STRUCTURE_TYPE_TEMP_CRITICAL_EXP_PROPERTIES, nullptr, -1.0};
        zes_temp_properties_t properties = {ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, &criticalProperties};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        EXPECT_EQ(&criticalProperties, properties.pNext);
        EXPECT_DOUBLE_EQ(0.0, criticalProperties.criticalTemperature);
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenOsTemperatureReportingCriticalTemperatureWhenGettingCriticalPropertiesThenReportedCriticalTemperatureIsReturned) {
    TemperatureImp temperatureImp(pOsSysman, false, 0u, ZES_TEMP_SENSORS_GPU, 0u);
    auto pMockOsTemperature = std::make_unique<MockWddmCriticalTemperatureImp>(pOsSysman);
    const double expectedCriticalTemperature = pMockOsTemperature->mockCriticalTemperature;
    temperatureImp.pOsTemperature = std::move(pMockOsTemperature);

    zes_intel_temp_critical_exp_properties_t criticalProperties = {ZES_INTEL_STRUCTURE_TYPE_TEMP_CRITICAL_EXP_PROPERTIES, nullptr, -1.0};
    zes_temp_properties_t properties = {ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, &criticalProperties};
    EXPECT_EQ(ZE_RESULT_SUCCESS, temperatureImp.temperatureGetProperties(&properties));
    EXPECT_DOUBLE_EQ(expectedCriticalTemperature, criticalProperties.criticalTemperature);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenUnknownStructureInPNextWhenGettingPropertiesThenInvalidArgumentIsReturned) {
    auto handles = getTempHandles(temperatureHandleComponentCount);
    for (auto handle : handles) {
        zes_base_properties_t unknownProperties = {ZES_STRUCTURE_TYPE_FORCE_UINT32, nullptr};
        zes_temp_properties_t properties = {ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, &unknownProperties};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesTemperatureGetProperties(handle, &properties));
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidOsTemperatureObjectWhenGettingTemperatureSensorThenCallSucceeds) {
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperTemp>();
    std::swap(pWddmSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    std::unique_ptr<PublicWddmTemperatureImp> pWddmTemperatureImp = std::make_unique<PublicWddmTemperatureImp>(pOsSysman);
    double mockedTemperature = 40;
    double temp = mockedTemperature;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pWddmTemperatureImp->getSensorTemperature(&temp));
    EXPECT_EQ(temp, mockedTemperature);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidOsTemperatureObjectWhenSettingTemperatureTypeThenCallSucceeds) {
    std::unique_ptr<PublicWddmTemperatureImp> pWddmTemperatureImp = std::make_unique<PublicWddmTemperatureImp>(pOsSysman);
    EXPECT_EQ(pWddmTemperatureImp->type, ZES_TEMP_SENSORS_GLOBAL);
    pWddmTemperatureImp->setSensorType(ZES_TEMP_SENSORS_GPU);
    EXPECT_EQ(pWddmTemperatureImp->type, ZES_TEMP_SENSORS_GPU);
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidOsTemperatureObjectWhenCheckingIfTempModuleSupportedThenCallSucceeds) {
    std::unique_ptr<PublicWddmTemperatureImp> pWddmTemperatureImp = std::make_unique<PublicWddmTemperatureImp>(pOsSysman);
    EXPECT_TRUE(pWddmTemperatureImp->isTempModuleSupported());
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandlesWhenCallingReInitOnTemperatureHandleContextThenHandlesRemainValid) {
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getTempHandles(count);
    ASSERT_EQ(count, static_cast<uint32_t>(handles.size()));

    pSysmanDeviceImp->pTempHandleContext->reInit();

    EXPECT_EQ(count, static_cast<uint32_t>(pSysmanDeviceImp->pTempHandleContext->handleList.size()));
    for (auto &handle : pSysmanDeviceImp->pTempHandleContext->handleList) {
        EXPECT_NE(nullptr, handle);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
