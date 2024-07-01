/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/temperature/windows/sysman_os_temperature_imp.h"
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
        std::vector<wchar_t> deviceInterfacePmt(pmtInterfaceName.begin(), pmtInterfaceName.end());
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new TemperatureKmdSysManager);
        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        auto pPmt = new PublicPlatformMonitoringTech(deviceInterfacePmt, pWddmSysmanImp->getSysmanProductHelper());
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

} // namespace ult
} // namespace Sysman
} // namespace L0
