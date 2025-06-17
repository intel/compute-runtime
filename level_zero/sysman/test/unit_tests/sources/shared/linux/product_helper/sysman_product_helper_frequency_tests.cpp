/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonPL2WhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL2File, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonPL4WhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;

    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonPL4File, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(throttleReason, ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleThermalReasonWhenCallingGetThrottleReasonsThenVerifyCorrectReasonIsReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t validReason = 1;
    pSysfsAccess->setValU32(throttleReasonStatusFile, validReason);
    pSysfsAccess->setValU32(throttleReasonThermalFile, validReason);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(throttleReason, unsetAllThrottleReasons);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenThrottleReasonsStatusSysfsReadFileFailsWhenCallingGetThrottleReasonsThenNoThrottleReasonsAreReturned, IsXeCore) {

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subdeviceId = 0;
    uint32_t unsetAllThrottleReasons = 0u;
    pSysfsAccess->mockReadUnsignedIntResult = ZE_RESULT_ERROR_UNKNOWN;

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
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

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pLinuxSysmanImp, subdeviceId);
    EXPECT_EQ(throttleReason, setAllThrottleReasonsExceptPL1);
}

HWTEST2_F(SysmanProductHelperFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateWithLegacyPathThenVerifyzesFrequencyGetStateTestCallSucceeds, IsXeCore) {

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
        zes_freq_state_t state;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(setAllThrottleReasons, state.throttleReasons);
        EXPECT_EQ(nullptr, state.pNext);
        EXPECT_LE(state.currentVoltage, 0);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
