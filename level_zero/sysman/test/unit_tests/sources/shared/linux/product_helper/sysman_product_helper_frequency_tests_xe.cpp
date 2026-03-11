/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/frequency/linux/mock_sysfs_frequency_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t invalidReasonValue = 0u;
constexpr uint32_t validReasonValue = 1u;
constexpr uint32_t handleComponentCount = 1u;

class SysmanProductHelperFrequencyTestFixture : public SysmanDeviceFixture {
  public:
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = nullptr;
    std::unique_ptr<MockXeFrequencySysfsAccess> pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccess = std::make_unique<MockXeFrequencySysfsAccess>();
        pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingFrequencySetRangeThenVerifyFrequencySetRangeTestReturnsError, IsCRI) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndFrequencyStatePnextPointerIsNotValidThenNoThrottleReasonsAreReturned, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(0u, state.throttleReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndStatusReasonFileReadFailedThenVerifyCallSucceedsWithProperValuesReturned, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
        throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        state.pNext = &throttleReasons;

        pSysfsAccess->mockThrottleReasonStatusReadSuccess = false;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(0u, state.throttleReasons);

        auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);
        EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndThrottleReasonStatusIsInvalidThenVerifyCallSucceedsWithProperValuesReturned, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
        throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        state.pNext = &throttleReasons;

        pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, invalidReasonValue);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        EXPECT_EQ(0u, state.throttleReasons);

        auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);
        EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndExtensionStructureTypeIsNotValidThenExpectNoDetailedReasonAndUtilizationLimitedIsEnabled, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
        throttleReasons.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        state.pNext = &throttleReasons;

        pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));

        uint32_t expectedAggregatedReasons = ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED;
        auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);
        EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
        EXPECT_EQ(expectedAggregatedReasons, state.throttleReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidProductHelperInstanceWhenCallingGetThrottleReasonsAndBaseDirectoryDoesNotExistThenExpectNoDetailedReasonAndUtilizationLimitedIsEnabled, IsCRI) {
    std::unique_ptr<PublicLinuxFrequencyImp> pLinuxFrequencyImp = std::make_unique<PublicLinuxFrequencyImp>(pOsSysman, 0, 0, ZES_FREQ_DOMAIN_GPU);
    zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
    throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
    state.pNext = &throttleReasons;

    pSysfsAccess->mockDirectoryExists = false;
    pSysfsAccess->mockThrottleReasonStatusReadForceSuccess = true;
    pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
    pSysfsAccess->mockDetailedReasonReadSuccess = false;

    state.throttleReasons = pSysmanProductHelper->getThrottleReasons(pSysmanKmdInterface, pSysfsAccess.get(), 0, const_cast<void *>(state.pNext));
    auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);

    uint32_t expectedAggregatedReasons = ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED;

    EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
    EXPECT_EQ(expectedAggregatedReasons, state.throttleReasons);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndThrottleReasonFileReadFailsThenExpectNoDetailedReasonAndUtilizationLimitedIsEnabled, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
        throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        state.pNext = &throttleReasons;

        pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
        pSysfsAccess->mockDetailedReasonReadSuccess = false;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);

        uint32_t expectedAggregatedReasons = ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED;

        EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
        EXPECT_EQ(expectedAggregatedReasons, state.throttleReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndNoThrottleReasonsAreSetThenExpectNoDetailedReasonAndUtilizationLimitedIsEnabled, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
        throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
        state.pNext = &throttleReasons;

        pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
        auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);

        uint32_t expectedAggregatedReasons = ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED;

        EXPECT_EQ(0u, pThrottleReasons->detailedReasons);
        EXPECT_EQ(expectedAggregatedReasons, state.throttleReasons);
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingZesFrequencyGetStateAndDetailedThrottleReasonsAreSetOrAvailableThenProperThrottleReasonsAreReturned, IsCRI) {
    auto handles = getFreqHandles(handleComponentCount);
    // Iterate over all combinations of the three mock flags (true/false)
    for (bool cardPL1 : {false, true}) {
        for (bool memoryThermal : {false, true}) {
            for (bool voltageP0 : {false, true}) {
                for (auto handle : handles) {
                    zes_intel_freq_throttle_detailed_reason_exp_t throttleReasons = {};
                    throttleReasons.stype = ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP;
                    zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE};
                    state.pNext = &throttleReasons;

                    // Set all sysfs values to valid
                    pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
                    pSysfsAccess->setValU32(detailedThrottleReasonCardPL1File, validReasonValue);
                    pSysfsAccess->setValU32(detailedThrottleReasonFastVMode, validReasonValue);
                    pSysfsAccess->setValU32(detailedThrottleReasonMemoryThermalFile, validReasonValue);
                    pSysfsAccess->setValU32(detailedThrottleReasonVoltageP0File, validReasonValue);

                    // Set the mock flags for this combination
                    pSysfsAccess->mockCardPL1DetailedReasonReadSuccess = (cardPL1);
                    pSysfsAccess->mockMemoryThermalDetailedReasonReadSuccess = (memoryThermal);
                    pSysfsAccess->mockVoltageP0DetailedReasonReadSuccess = (voltageP0);
                    pSysfsAccess->mockFastVModeDetailedReasonReadSuccess = true;

                    uint32_t expectedDetailedReasons = 0u;
                    uint32_t expectedAggregatedReasons = 0u;

                    if (cardPL1) {
                        expectedDetailedReasons |= ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL1;
                        expectedAggregatedReasons |= static_cast<zes_freq_throttle_reason_flags_t>(ZES_FREQ_THROTTLE_REASON_FLAG_POWER);
                    }
                    if (memoryThermal) {
                        expectedDetailedReasons |= ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_MEMORY;
                        expectedAggregatedReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL;
                    }
                    if (voltageP0) {
                        expectedDetailedReasons |= ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_VOLTAGE_P0_FREQ;
                        expectedAggregatedReasons |= static_cast<zes_freq_throttle_reason_flags_t>(ZES_FREQ_THROTTLE_REASON_FLAG_VOLTAGE);
                    }

                    expectedDetailedReasons |= ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_FAST_VMODE;
                    expectedAggregatedReasons |= static_cast<zes_freq_throttle_reason_flags_t>(ZES_FREQ_THROTTLE_REASON_FLAG_POWER);

                    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));
                    EXPECT_EQ(expectedAggregatedReasons, state.throttleReasons);

                    auto pThrottleReasons = reinterpret_cast<const zes_intel_freq_throttle_detailed_reason_exp_t *>(state.pNext);
                    EXPECT_EQ(expectedDetailedReasons, pThrottleReasons->detailedReasons);
                }
            }
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
