/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_temperature.h"

namespace L0 {
namespace ult {
class SysmanTemperatureFixture : public DeviceFixture, public ::testing::Test {
  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    OsTemperature *pOsTemperature = nullptr;
    PublicLinuxTemperatureImp linuxTemperatureImp;
    TemperatureImp *pTemperatureImp = nullptr;
    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pOsTemperature = static_cast<OsTemperature *>(&linuxTemperatureImp);
        pTemperatureImp = new TemperatureImp();
        pTemperatureImp->pOsTemperature = pOsTemperature;
        pTemperatureImp->init();
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        //pOsTemperature is static_cast of LinuxTemperatureImp class , hence in cleanup assign to nullptr
        pTemperatureImp->pOsTemperature = nullptr;
        if (pTemperatureImp != nullptr) {
            delete pTemperatureImp;
            pTemperatureImp = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanTemperatureFixture, GivenValidOSTempHandleWhenCheckingForTempSupportThenExpectFalseToBeReturned) {
    EXPECT_FALSE(pOsTemperature->isTempModuleSupported());
}

TEST_F(SysmanTemperatureFixture, GivenValidOSTemperatureHandleWhenSettingTemperatureSensorThenSameSetSensorIsRetrieved) {
    pOsTemperature->setSensorType(ZET_TEMP_SENSORS_GPU);
    EXPECT_EQ(linuxTemperatureImp.type, ZET_TEMP_SENSORS_GPU);

    pOsTemperature->setSensorType(ZET_TEMP_SENSORS_GLOBAL);
    EXPECT_EQ(linuxTemperatureImp.type, ZET_TEMP_SENSORS_GLOBAL);
}

TEST_F(SysmanTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanTemperatureGet(hSysman, &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
    uint32_t testcount = count + 1;
    result = zetSysmanTemperatureGet(hSysman, &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);
}
} // namespace ult
} // namespace L0
