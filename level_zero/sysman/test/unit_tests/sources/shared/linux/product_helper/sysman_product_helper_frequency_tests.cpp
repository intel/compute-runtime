/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/frequency/linux/mock_sysman_frequency.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanProductHelperFrequencyFixture : public SysmanDeviceFixture {
  public:
    MockFrequencySysfsAccess *pSysfsAccess = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccess = new MockFrequencySysfsAccess();
        auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

using IsProductXeHpSdvPlus = IsAtLeastProduct<IGFX_XE_HP_SDV>;

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenFrequencyModuleWhenGettingStepSizeThenValidStepSizeIsReturned, IsProductXeHpSdvPlus) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    double stepSize = 0;
    pSysmanProductHelper->getFrequencyStepSize(&stepSize);
    EXPECT_EQ(50.0, stepSize);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenFrequencyModuleWhenQueryingFrequencySetRangeSupportedThenVerifySetRangeIsSupported, IsXeCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(true, pSysmanProductHelper->isFrequencySetRangeSupported());
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonPL1WhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonPL2WhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonPL4WhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleThermalReasonWhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenValidPL4AndInvalidThermalThrottleReasonWhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, invalidReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenValidPL1PL2PL4AndThermalThrottleReasonsWhenCallingGetThrottleReasonsThenCorrectReasonsAreReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t setAllThrottleReasons = (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
                                      ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, setAllThrottleReasons);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenNoThrottleReasonsStatusWhenCallingGetThrottleReasonsThenNoThrottleReasonsAreReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t unsetAllThrottleReasons = 0u;
    pSysfsAccess->setValU32(throttleReasonStatusFile, invalidReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, unsetAllThrottleReasons);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsStatusSysfsReadFileFailsWhenCallingGetThrottleReasonsThenNoThrottleReasonsAreReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t unsetAllThrottleReasons = 0u;
    pSysfsAccess->mockReadUnsignedIntResult = ZE_RESULT_ERROR_UNKNOWN;

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, unsetAllThrottleReasons);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsExceptThermalWhenCallingGetThrottleReasonsThenCorrectThrottleReasonsForMissingThermalStatusIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    pSysfsAccess->mockReadThermalResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptThermal =
        (ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, invalidReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, setAllThrottleReasonsExceptThermal);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsPL4WhenCallingGetThrottleReasonsThenCorrectThrottleReasonsForMissingPL4StatusIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    pSysfsAccess->mockReadPL4Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL4 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, invalidReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, setAllThrottleReasonsExceptPL4);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsPL2WhenCallingGetThrottleReasonsThenCorrectThrottleReasonsForMissingPL2StatusIsReturned, IsXeCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    pSysfsAccess->mockReadPL2Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL2 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, invalidReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, setAllThrottleReasonsExceptPL2);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsPL1WhenCallingGetThrottleReasonsThenCorrectThrottleReasonsForMissingPL1StatusIsReturned, IsXeCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    pSysfsAccess->mockReadPL1Result = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t validReason = 1;
    uint32_t invalidReason = 0;
    uint32_t setAllThrottleReasonsExceptPL1 =
        (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
         ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP);

    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL1File, invalidReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp->getSysmanKmdInterface(), &pLinuxSysmanImp->getSysfsAccess(), subdeviceId, nullptr);
    EXPECT_EQ(throttleReason, setAllThrottleReasonsExceptPL1);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateWithLegacyPathThenVerifyCallSucceeds, IsXeCore) {

    pSysfsAccess->isLegacy = true;
    pSysfsAccess->directoryExistsResult = false;

    uint32_t handleCount = 1u;
    uint32_t count = 0u;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleCount);

    std::vector<zes_freq_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        const double testRequestValue = 400.0;
        const double testTdpValue = 1100.0;
        const double testEfficientValue = 300.0;
        const double testActualValue = 550.0;
        uint32_t validReason = 1;
        uint32_t setAllThrottleReasons = (ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP |
                                          ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);

        pSysfsAccess->setValLegacy(requestFreqFileLegacy, testRequestValue);
        pSysfsAccess->setValLegacy(tdpFreqFileLegacy, testTdpValue);
        pSysfsAccess->setValLegacy(actualFreqFileLegacy, testActualValue);
        pSysfsAccess->setValLegacy(efficientFreqFileLegacy, testEfficientValue);
        pSysfsAccess->setValU32Legacy(throttleReasonStatusFileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL1FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL2FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonPL4FileLegacy, validReason);
        pSysfsAccess->setValU32Legacy(throttleReasonThermalFileLegacy, validReason);
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(setAllThrottleReasons, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

using IsNotCRI = IsNoneProducts<IGFX_CRI>;

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesThenNonZeroCountIsReturnedAndCallSucceds, IsNotCRI) {
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroAndValidPtrWhenEnumeratingFrequencyHandlesThenNonZeroCountAndNoHandlesAreReturnedAndCallSucceds, IsNotCRI) {
    uint32_t count = 0U;
    zes_freq_handle_t handle = static_cast<zes_freq_handle_t>(0UL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, &handle));
    EXPECT_EQ(count, handleComponentCount);
    EXPECT_EQ(handle, static_cast<zes_freq_handle_t>(0UL));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesAndMediaFreqDomainIsPresentThenNonZeroCountIsReturnedAndCallSucceds, IsNotCRI) {
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->capabilityTable.supportsImages = true;
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 2U);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesAndMediaDomainIsAbsentThenNonZeroCountIsReturnedAndCallSucceds, IsNotCRI) {
    pSysfsAccess->directoryExistsResult = false;
    pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->capabilityTable.supportsImages = true;
    uint32_t count = 0U;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, 1U);

    uint32_t testCount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &testCount, nullptr));
    EXPECT_EQ(count, testCount);

    auto handles = getFreqHandles(count);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingZesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingZesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        uint32_t count = 0;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);

        double *clocks = new double[count];
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, clocks));
        EXPECT_EQ(numClocks, count);
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_DOUBLE_EQ(clockValue(minFreq + (step * i)), clocks[i]);
        }
        delete[] clocks;
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    pSysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingZesFrequencySetRangeWithNewMaxLessThanOldMinThenVerifyCallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(newMax, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenNegativeUnityRangeSetWhenGetRangeIsCalledThenReturnsValueFromDefaultPath, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_EQ(1, limits.min);
        EXPECT_EQ(1000, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingZesFrequencySetRangeWithNewMinGreaterThanOldMaxThenVerifyCallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        pSysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(newMin, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenInvalidFrequencyLimitsWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTestReturnsError, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits;

    // Verify that Max must be greater than min range.
    limits.min = clockValue(maxFreq + step);
    limits.max = minFreq;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingZesFrequencySetRangeWithLegacyPathThenVerifyCallSucceeds, IsXeCore) {
    pSysfsAccess->isLegacy = true;
    pSysfsAccess->directoryExistsResult = false;
    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    pSysmanDeviceImp->pFrequencyHandleContext->init(pLinuxSysmanImp->getSubDeviceCount());
    auto handles = getFreqHandles(handleComponentCount);
    double minFreqLegacy = 400.0;
    double maxFreqLegacy = 1200.0;
    pSysfsAccess->setValLegacy(minFreqFileLegacy, minFreqLegacy);
    pSysfsAccess->setValLegacy(maxFreqFileLegacy, maxFreqLegacy);
    for (auto handle : handles) {
        zes_freq_range_t limits;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreqLegacy, limits.min);
        EXPECT_DOUBLE_EQ(maxFreqLegacy, limits.max);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(pSysfsAccess->mockBoost, limits.max);
    }
}

HWTEST2_F(FreqMultiDeviceFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetPropertiesThenSuccessIsReturned, IsNotCRI) {
    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr));
    EXPECT_EQ(count, multiHandleComponentCount);
    auto handles = getFreqHandles(multiHandleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(nullptr, properties.pNext);
        EXPECT_EQ(ZES_FREQ_DOMAIN_GPU, properties.type);
        EXPECT_TRUE(properties.onSubdevice);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
