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
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/temperature/temperature_imp.h"

using ::testing::_;
using ::testing::NiceMock;

namespace L0 {
namespace ult {
constexpr uint32_t handleCount = 2u; // We are creating two temp handles
const std::string deviceName("testDevice");
class SysmanTemperatureFixture : public DeviceFixture, public ::testing::Test {
  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    Mock<TemperaturePmt> *pPmt = nullptr;

    TemperatureImp *pTemperatureImp[handleCount] = {};
    zet_sysman_temp_handle_t hSysmanTempHandle[handleCount];
    PublicLinuxTemperatureImp linuxTemperatureImp[handleCount];

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pPmt = new NiceMock<Mock<TemperaturePmt>>;
        FsAccess *pFsAccess = nullptr;
        pPmt->setVal(true);
        pPmt->init(deviceName, pFsAccess);

        for (uint32_t i = 0; i < handleCount; i++) {
            linuxTemperatureImp[i].pPmt = pPmt;
            OsTemperature *pOsTemperature = static_cast<OsTemperature *>(&linuxTemperatureImp[i]);
            pTemperatureImp[i] = new TemperatureImp();
            pTemperatureImp[i]->pOsTemperature = pOsTemperature;
            pTemperatureImp[i]->init();
            pTemperatureImp[i]->pOsTemperature->setSensorType(static_cast<zet_temp_sensors_t>(i));
            sysmanImp->pTempHandleContext->handleList.push_back(pTemperatureImp[i]);
            hSysmanTempHandle[i] = pTemperatureImp[i]->toHandle();
        }
        hSysman = sysmanImp->toHandle();
    }
    void TearDown() override {
        // pOsTemperatureGlobal is static_cast of LinuxTemperatureImp class , hence in cleanup assign to nullptr
        pTemperatureImp[0]->pOsTemperature = nullptr;
        pTemperatureImp[1]->pOsTemperature = nullptr;

        if (pPmt != nullptr) {
            delete pPmt;
            pPmt = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zetSysmanTemperatureGet(hSysman, &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleCount);
    uint32_t testcount = count + 1;
    result = zetSysmanTemperatureGet(hSysman, &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, count);
}

TEST_F(SysmanTemperatureFixture, GivenValidTempHandleWhenGettingGPUTemperatureThenValidTemperatureReadingsRetrieved) {
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanTemperatureGetState(hSysmanTempHandle[1], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(tempArr[computeIndex]));
}

TEST_F(SysmanTemperatureFixture, GivenValidTempHandleWhenGettingGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    double temperature;
    ASSERT_EQ(ZE_RESULT_SUCCESS, zetSysmanTemperatureGetState(hSysmanTempHandle[0], &temperature));
    EXPECT_EQ(temperature, static_cast<double>(tempArr[globalIndex]));
}

TEST_F(SysmanTemperatureFixture, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    double pTemperature = 0;
    pTemperatureImp[0]->pOsTemperature->setSensorType(ZET_TEMP_SENSORS_MEMORY);
    pTemperatureImp[1]->pOsTemperature->setSensorType(ZET_TEMP_SENSORS_MEMORY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pTemperatureImp[0]->pOsTemperature->getSensorTemperature(&pTemperature));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pTemperatureImp[1]->pOsTemperature->getSensorTemperature(&pTemperature));
}

TEST_F(SysmanTemperatureFixture, GivenPmtSupportNotAvailableWhenCheckingForTempModuleSupportThenTempModuleSupportShouldReturnFalse) {
    //delete previously allocated memory, allocated by init() call in SetUp()
    if (pPmt->mappedMemory != nullptr) {
        delete pPmt->mappedMemory;
        pPmt->mappedMemory = nullptr;
    }
    pPmt->setVal(false);
    FsAccess *pFsAccess = nullptr;
    pPmt->init(deviceName, pFsAccess);
    EXPECT_FALSE(pTemperatureImp[0]->pOsTemperature->isTempModuleSupported());
    EXPECT_FALSE(pTemperatureImp[1]->pOsTemperature->isTempModuleSupported());
}

TEST_F(SysmanTemperatureFixture, GivenPmtSupportAvailableWhenCheckingForTempModuleSupportThenTempModuleSupportShouldReturnTrue) {
    //delete previously allocated memory, allocated by init() call in SetUp()
    if (pPmt->mappedMemory != nullptr) {
        delete pPmt->mappedMemory;
        pPmt->mappedMemory = nullptr;
    }
    pPmt->setVal(true);
    FsAccess *pFsAccess = nullptr;
    pPmt->init(deviceName, pFsAccess);
    EXPECT_TRUE(pTemperatureImp[0]->pOsTemperature->isTempModuleSupported());
    EXPECT_TRUE(pTemperatureImp[1]->pOsTemperature->isTempModuleSupported());
}

} // namespace ult
} // namespace L0
