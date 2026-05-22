/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/frequency/linux/mock_sysfs_frequency_xe.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mocks/mock_sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {
namespace ult {

using IsNotCRI = IsNoneProducts<IGFX_CRI>;

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

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenRatlReasonIsSetWhenCallingGetThrottleReasonsThenPsuAlertFlagIsReturned, IsCRI) {
    pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
    pSysfsAccess->setValU32(ratlReasonFile, validReasonValue);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pSysmanKmdInterface, pSysfsAccess.get(), 0, nullptr);

    zes_freq_throttle_reason_flags_t expectedReasons = ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT |
                                                       static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED);
    EXPECT_EQ(expectedReasons, throttleReason);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenRatlReasonIsZeroWhenCallingGetThrottleReasonsThenPsuAlertFlagIsNotReturned, IsCRI) {
    pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
    pSysfsAccess->setValU32(ratlReasonFile, invalidReasonValue);

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pSysmanKmdInterface, pSysfsAccess.get(), 0, nullptr);

    EXPECT_EQ(0u, throttleReason & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenRatlReasonReadFailsWhenCallingGetThrottleReasonsThenPsuAlertFlagIsNotReturned, IsCRI) {
    pSysfsAccess->setValU32(detailedThrottleReasonStatusFile, validReasonValue);
    pSysfsAccess->mockRatlReasonReadError = true;

    zes_freq_throttle_reason_flags_t throttleReason = pSysmanProductHelper->getThrottleReasons(pSysmanKmdInterface, pSysfsAccess.get(), 0, nullptr);

    EXPECT_EQ(0u, throttleReason & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidProductHelperInstanceWhenQueryingMemoryDomainSupportThenMemoryDomainIsSupported, IsCRI) {
    EXPECT_TRUE(pSysmanProductHelper->isMemoryDomainSupported());
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidDeviceHandleWhenEnumeratingFrequencyDomainsWithNoImageSupportThenMemoryDomainIsIncluded, IsCRI) {
    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(2u, count);

    auto handles = getFreqHandles(count);
    EXPECT_EQ(2u, handles.size());

    bool hasGpuDomain = false;
    bool hasMemoryDomain = false;

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));

        if (properties.type == ZES_FREQ_DOMAIN_GPU) {
            hasGpuDomain = true;
        } else if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            hasMemoryDomain = true;
        }
    }

    EXPECT_TRUE(hasGpuDomain);
    EXPECT_TRUE(hasMemoryDomain);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidDeviceHandleWhenEnumeratingFrequencyDomainsWithImageSupportAndNoMediaDirectoryThenMemoryDomainIsIncluded, IsCRI) {
    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = true;

    pSysfsAccess->mockMediaDirectoryExists = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(2u, count);

    auto handles = getFreqHandles(count);
    EXPECT_EQ(2u, handles.size());

    bool hasGpuDomain = false;
    bool hasMemoryDomain = false;
    bool hasMediaDomain = false;

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));

        if (properties.type == ZES_FREQ_DOMAIN_GPU) {
            hasGpuDomain = true;
        } else if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            hasMemoryDomain = true;
        } else if (properties.type == ZES_FREQ_DOMAIN_MEDIA) {
            hasMediaDomain = true;
        }
    }

    EXPECT_TRUE(hasGpuDomain);
    EXPECT_TRUE(hasMemoryDomain);
    EXPECT_FALSE(hasMediaDomain);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidDeviceHandleWhenEnumeratingFrequencyDomainsWithImageSupportAndMediaDirectoryThenAllDomainsAreIncluded, IsCRI) {
    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = true;

    pSysfsAccess->mockMediaDirectoryExists = true;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_EQ(3u, count);

    auto handles = getFreqHandles(count);
    EXPECT_EQ(3u, handles.size());

    bool hasGpuDomain = false;
    bool hasMemoryDomain = false;
    bool hasMediaDomain = false;

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));

        if (properties.type == ZES_FREQ_DOMAIN_GPU) {
            hasGpuDomain = true;
        } else if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            hasMemoryDomain = true;
        } else if (properties.type == ZES_FREQ_DOMAIN_MEDIA) {
            hasMediaDomain = true;
        }
    }

    EXPECT_TRUE(hasGpuDomain);
    EXPECT_TRUE(hasMemoryDomain);
    EXPECT_TRUE(hasMediaDomain);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingGetPropertiesOnMemoryDomainThenCorrectPropertiesAreReturned, IsCRI) {
    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getFreqHandles(count);

    zes_freq_handle_t memoryHandle = nullptr;
    for (auto handle : handles) {
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            memoryHandle = handle;
            EXPECT_EQ(nullptr, properties.pNext);
            EXPECT_EQ(ZES_FREQ_DOMAIN_MEMORY, properties.type);
            EXPECT_FALSE(properties.onSubdevice);
            EXPECT_FALSE(properties.canControl);
            break;
        }
    }

    EXPECT_NE(nullptr, memoryHandle);
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingFrequencyGetRangeOnMemoryDomainThenUnsupportedValuesAreReturned, IsCRI) {
    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getFreqHandles(count);

    for (auto handle : handles) {
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            zes_freq_range_t limits = {};
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
            EXPECT_DOUBLE_EQ(-1.0, limits.min);
            EXPECT_DOUBLE_EQ(-1.0, limits.max);
            break;
        }
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingFrequencySetRangeOnMemoryDomainThenUnsupportedFeatureIsReturned, IsCRI) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_MEMORY);
    zes_freq_range_t limits = {};
    limits.min = 1000.0;
    limits.max = 2000.0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenCallingFrequencyGetStateOnMemoryDomainThenValidValuesAreReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::string strPathName(pathname);
        if (strPathName == "/sys/class/intel_pmt/telem1/offset") {
            return 4;
        } else if (strPathName == "/sys/class/intel_pmt/telem1/guid") {
            return 5;
        } else if (strPathName == "/sys/class/intel_pmt/telem1/telem") {
            return 6;
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 4) {
            std::string offsetValue = "0";
            memcpy(buf, offsetValue.data(), count);
            return count;
        } else if (fd == 5) {
            std::string guidValue = "0x1e2fa030";
            memcpy(buf, guidValue.data(), count);
            return count;
        } else if (fd == 6) {
            if (offset == 56) {
                uint32_t frequencyData = 2000;
                memcpy(buf, &frequencyData, count);
                return count;
            } else if (offset == 60) {
                uint32_t voltageData = 850;
                memcpy(buf, &voltageData, count);
                return count;
            }
        }
        return -1;
    });

    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getFreqHandles(count);

    for (auto handle : handles) {
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            zes_freq_state_t state = {};
            state.stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));

            EXPECT_GT(state.actual, 0.0);
            EXPECT_GT(state.currentVoltage, 0.0);
            EXPECT_EQ(-1.0, state.request);
            EXPECT_EQ(-1.0, state.tdp);
            EXPECT_EQ(-1.0, state.efficient);
            EXPECT_EQ(0u, state.throttleReasons);
            break;
        }
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenPmtReadValueFailsForMemoryDomainThenActualFrequencyAndVoltageReturnErrorValues, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::map<std::string, std::string> fileNameLinkMap = {
            {"/sys/class/intel_pmt/telem1", "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
        };
        auto it = fileNameLinkMap.find(std::string(path));
        if (it != fileNameLinkMap.end()) {
            std::memcpy(buf, it->second.c_str(), it->second.size());
            return static_cast<int>(it->second.size());
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::string strPathName(pathname);
        if (strPathName == "/sys/class/intel_pmt/telem1/offset") {
            return 4;
        } else if (strPathName == "/sys/class/intel_pmt/telem1/guid") {
            return 5;
        } else if (strPathName == "/sys/class/intel_pmt/telem1/telem") {
            return 6;
        }
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 4) {
            std::string offsetValue = "0";
            memcpy(buf, offsetValue.data(), count);
            return count;
        } else if (fd == 5) {
            std::string guidValue = "0x1e2fa030";
            memcpy(buf, guidValue.data(), count);
            return count;
        }
        return -1;
    });

    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getFreqHandles(count);

    for (auto handle : handles) {
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            zes_freq_state_t state = {};
            state.stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));

            EXPECT_EQ(-1.0, state.actual);
            EXPECT_EQ(0.0, state.currentVoltage);
            break;
        }
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidFrequencyHandleWhenBuildKeyOffsetMapFailsForMemoryDomainThenActualFrequencyAndVoltageReturnErrorValues, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });

    auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.supportsImages = false;

    for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();

    uint32_t count = 0U;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(pSysmanDevice->toHandle(), &count, nullptr));
    auto handles = getFreqHandles(count);

    for (auto handle : handles) {
        zes_freq_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        if (properties.type == ZES_FREQ_DOMAIN_MEMORY) {
            zes_freq_state_t state = {};
            state.stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));

            EXPECT_EQ(-1.0, state.actual);
            EXPECT_EQ(0.0, state.currentVoltage);
            break;
        }
    }
}

HWTEST2_F(SysmanProductHelperFrequencyTestFixture, GivenValidProductHelperInstanceWhenCallingFrequencyMethodsDirectlyForMemoryDomainThenUnsupportedFeatureIsReturned, IsNotCRI) {
    double actual = 0.0;
    double voltage = 0.0;
    uint32_t subdeviceId = 0;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              pSysmanProductHelper->getActualFrequency(pLinuxSysmanImp, ZES_FREQ_DOMAIN_MEMORY, subdeviceId, &actual));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
              pSysmanProductHelper->getCurrentVoltage(pLinuxSysmanImp, ZES_FREQ_DOMAIN_MEMORY, subdeviceId, &voltage));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenComponentCountZeroWhenEnumeratingFrequencyHandlesThenNonZeroCountIsReturnedAndCallSucceds, IsNotCRI) {
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

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenComponentCountZeroAndValidPtrWhenEnumeratingFrequencyHandlesThenNonZeroCountAndNoHandlesAreReturnedAndCallSucceds, IsNotCRI) {
    uint32_t count = 0U;
    zes_freq_handle_t handle = static_cast<zes_freq_handle_t>(0UL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumFrequencyDomains(device->toHandle(), &count, &handle));
    EXPECT_EQ(count, handleComponentCount);
    EXPECT_EQ(handle, static_cast<zes_freq_handle_t>(0UL));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCountIsMoreThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 80;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
        EXPECT_EQ(numClocks, count);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndZeroCountWhenCountIsLessThanNumClocksThenCallSucceeds, IsPVC) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 20;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds, IsPVC) {
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

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures1ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = minFreq;
    limits.max = 600.0;
    sysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    sysfsAccess->mockWriteMinResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyLimitsWhenCallingFrequencySetRangeForFailures2ThenAPIExitsGracefully, IsXeCore) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    auto pFrequencyImp = std::make_unique<L0::Sysman::FrequencyImp>(pOsSysman, onSubdevice, subdeviceId, ZES_FREQ_DOMAIN_GPU);
    zes_freq_range_t limits = {};

    // Verify that Max must be within range.
    limits.min = 900.0;
    limits.max = maxFreq;
    sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pFrequencyImp->frequencySetRange(&limits));

    sysfsAccess->mockWriteMaxResult = ZE_RESULT_ERROR_UNKNOWN;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pFrequencyImp->frequencySetRange(&limits));
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(minFreqFile, startingMin);
        // If the new Max value is less than the old Min
        // value, the new Min must be set before the new Max
        limits.min = minFreq;
        limits.max = newMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(minFreq, limits.min);
        EXPECT_DOUBLE_EQ(newMax, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenNegativeRangeWhenSetRangeIsCalledAndGettingDefaultMaxValueFailsThenNoFreqRangeIsInEffect, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto &handle : handles) {
        const double negativeMin = -1;
        const double negativeMax = -1;
        zes_freq_range_t limits;

        limits.min = negativeMin;
        limits.max = negativeMax;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(-1, limits.min);
        EXPECT_DOUBLE_EQ(-1, limits.max);
    }
}

HWTEST2_F(SysmanDeviceFrequencyFixtureXe, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds, IsXeCore) {
    auto handles = getFreqHandles(handleComponentCount);
    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;
        zes_freq_range_t limits;

        sysfsAccess->setVal(maxFreqFile, startingMax);
        // If the new Min value is greater than the old Max
        // value, the new Max must be set before the new Min
        limits.min = newMin;
        limits.max = maxFreq;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
        EXPECT_DOUBLE_EQ(newMin, limits.min);
        EXPECT_DOUBLE_EQ(maxFreq, limits.max);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
