/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/frequency/windows/os_frequency_imp.h"
#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/frequency/windows/mock_frequency.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t frequencyHandleComponentCount = 2u;
constexpr double minFreq = 400.0;
constexpr double maxFreq = 1200.0;
constexpr double step = 50.0 / 3;
constexpr uint32_t numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;
class SysmanDeviceFrequencyFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<FrequencyKmdSysManager>> pKmdSysManager;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pKmdSysManager.reset(new Mock<FrequencyKmdSysManager>);

        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
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

    double clockValue(const double calculatedClock) {
        // i915 specific. frequency step is a fraction
        // However, the Kmd represents all clock
        // rates as integer values. So clocks are
        // rounded to the nearest integer.
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> get_frequency_handles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, frequencyHandleComponentCount);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenInvalidComponentCountWhenEnumeratingFrequencyDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, frequencyHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, frequencyHandleComponentCount);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyDomainsAndRequestSingleFailsThenZeroHandlesAreCreated) {
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNKNOWN;
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0);
}

TEST_F(SysmanDeviceFrequencyFixture, GivenComponentCountZeroWhenEnumeratingFrequencyDomainsThenValidPowerHandlesIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, frequencyHandleComponentCount);

    std::vector<zes_freq_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetPropertiesThenSuccessIsReturned) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(pKmdSysManager->mockDomainType[domainIndex], properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(pKmdSysManager->mockGPUCanControl[domainIndex], properties.canControl);
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockRp0[domainIndex], properties.max);
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockRpn[domainIndex], properties.min);
        } else if (domainIndex == ZES_FREQ_DOMAIN_MEMORY) {
            EXPECT_DOUBLE_EQ(-1, properties.max);
            EXPECT_DOUBLE_EQ(-1, properties.min);
        }
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesAllowSetCallsToFalseFrequencyGetPropertiesThenSuccessIsReturned) {
    pKmdSysManager->allowSetCalls = false;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(pKmdSysManager->mockDomainType[domainIndex], properties.type);
        EXPECT_FALSE(properties.onSubdevice);
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockRp0[domainIndex], properties.max);
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockRpn[domainIndex], properties.min);
        } else if (domainIndex == ZES_FREQ_DOMAIN_MEMORY) {
            EXPECT_DOUBLE_EQ(-1.0, properties.max);
            EXPECT_DOUBLE_EQ(-1.0, properties.min);
        }
        EXPECT_EQ(pKmdSysManager->mockGPUCannotControl[domainIndex], properties.canControl);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhileGettingFrequencyPropertiesAndRequestMultipleFailsThenInvalidPropertiesAreReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->requestMultipleSizeDiff = false;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_INVALID_SIZE;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(-1.0, properties.min);
        EXPECT_EQ(-1.0, properties.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhileGettingFrequencyPropertiesAndRequestMultipleReturnsInvalidSizeThenInvalidpropertiesAreReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->requestMultipleSizeDiff = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_SUCCESS;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_freq_properties_t properties;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetProperties(handle, &properties));
        EXPECT_EQ(-1.0, properties.min);
        EXPECT_EQ(-1.0, properties.max);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndZeroCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
        uint32_t count = 0;
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
            EXPECT_EQ(numClocks, count);
        } else if (domainIndex == ZES_FREQ_DOMAIN_MEMORY) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
            EXPECT_EQ(1, count);
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAndCorrectCountWhenCallingzesFrequencyGetAvailableClocksThenCallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        uint32_t count = 0;
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, nullptr));
            EXPECT_EQ(numClocks, count);

            double *clocks = new double[count];
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetAvailableClocks(handle, &count, clocks));
            EXPECT_EQ(numClocks, count);
            for (uint32_t i = 0; i < count; i++) {
                EXPECT_DOUBLE_EQ(clockValue(pKmdSysManager->mockRpn[domainIndex] + (step * i)), clocks[i]);
            }
            delete[] clocks;
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetRangeThenVerifyzesFrequencyGetRangeTestCallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        zes_freq_range_t limits;
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockMinFrequencyRange, limits.min);
            EXPECT_DOUBLE_EQ(pKmdSysManager->mockMaxFrequencyRange, limits.max);
        } else if (domainIndex == ZES_FREQ_DOMAIN_MEMORY) {
            EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesFrequencyGetRange(handle, &limits));
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest1CallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        const double startingMin = 900.0;
        const double newMax = 600.0;
        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            zes_freq_range_t limits;

            pKmdSysManager->mockMinFrequencyRange = static_cast<uint32_t>(startingMin);

            // If the new Max value is less than the old Min
            // value, the new Min must be set before the new Max
            limits.min = minFreq;
            limits.max = newMax;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
            EXPECT_DOUBLE_EQ(minFreq, limits.min);
            EXPECT_DOUBLE_EQ(newMax, limits.max);
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencySetRangeThenVerifyzesFrequencySetRangeTest2CallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        const double startingMax = 600.0;
        const double newMin = 900.0;

        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            zes_freq_range_t limits;

            pKmdSysManager->mockMaxFrequencyRange = static_cast<uint32_t>(startingMax);

            // If the new Min value is greater than the old Max
            // value, the new Max must be set before the new Min
            limits.min = newMin;
            limits.max = maxFreq;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencySetRange(handle, &limits));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetRange(handle, &limits));
            EXPECT_DOUBLE_EQ(newMin, limits.min);
            EXPECT_DOUBLE_EQ(maxFreq, limits.max);
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetStateThenVerifyCallSucceeds) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    uint32_t domainIndex = 0;

    for (auto handle : handles) {
        zes_freq_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyGetState(handle, &state));

        if (domainIndex == ZES_FREQ_DOMAIN_GPU) {
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockResolvedFrequency[domainIndex]), state.actual);
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockCurrentVoltage) / milliVoltsFactor, state.currentVoltage);
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockEfficientFrequency), state.efficient);
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockRequestedFrequency), state.request);
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockTdpFrequency), state.tdp);
            EXPECT_EQ(pKmdSysManager->mockThrottleReasons, state.throttleReasons);
        } else if (domainIndex == ZES_FREQ_DOMAIN_MEMORY) {
            EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockResolvedFrequency[domainIndex]), state.actual);
            EXPECT_DOUBLE_EQ(-1.0, state.currentVoltage);
            EXPECT_DOUBLE_EQ(-1.0, state.efficient);
            EXPECT_DOUBLE_EQ(-1.0, state.request);
            EXPECT_DOUBLE_EQ(-1.0, state.tdp);
            EXPECT_EQ(pKmdSysManager->mockThrottleReasons, state.throttleReasons);
        }

        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhileGettingFrequencyStateAndRequestMultipleFailsThenFailureIsReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_INVALID_SIZE;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zesFrequencyGetState(handle, &state));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhileGettingFrequencyStateAndRequestMultipleReturnsInvalidSizeThenFailureIsReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->requestMultipleSizeDiff = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_INVALID_SIZE;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_freq_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zesFrequencyGetState(handle, &state));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyGetThrottleTimeThenVerifyCallFails) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_freq_throttle_time_t throttletime = {};
        EXPECT_NE(ZE_RESULT_SUCCESS, zesFrequencyGetThrottleTime(handle, &throttletime));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetCapabilitiesThenVerifyCallSucceeds) {
    pKmdSysManager->allowSetCalls = false;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_oc_capabilities_t ocCaps = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetCapabilities(handle, &ocCaps));
        EXPECT_EQ(pKmdSysManager->mockIsExtendedModeSupported[domainIndex], ocCaps.isExtendedModeSupported);
        EXPECT_EQ(pKmdSysManager->mockIsFixedModeSupported[domainIndex], ocCaps.isFixedModeSupported);
        EXPECT_EQ(pKmdSysManager->mockHighVoltageSupported[domainIndex], ocCaps.isHighVoltModeCapable);
        EXPECT_EQ(pKmdSysManager->mockHighVoltageEnabled[domainIndex], ocCaps.isHighVoltModeEnabled);
        EXPECT_EQ(pKmdSysManager->mockIsIccMaxSupported, ocCaps.isIccMaxSupported);
        EXPECT_EQ(pKmdSysManager->mockIsOcSupported[domainIndex], ocCaps.isOcSupported);
        EXPECT_EQ(pKmdSysManager->mockIsTjMaxSupported, ocCaps.isTjMaxSupported);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockMaxFactoryDefaultFrequency[domainIndex]), ocCaps.maxFactoryDefaultFrequency);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockMaxFactoryDefaultVoltage[domainIndex]), ocCaps.maxFactoryDefaultVoltage);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockMaxOcFrequency[domainIndex]), ocCaps.maxOcFrequency);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockMaxOcVoltage[domainIndex]), ocCaps.maxOcVoltage);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockVoltageOffset[domainIndex]), ocCaps.maxOcVoltageOffset);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockVoltageOffset[domainIndex]), ocCaps.minOcVoltageOffset);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetFrequencyTargetThenVerifyCallSucceeds) {
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetFrequencyTarget(handle, &freqTarget));
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockFrequencyTarget[domainIndex]), freqTarget);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetFrequencyTargetAndRequestSingleFailsThenFailureIsReturned) {
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        pKmdSysManager->mockRequestSingle = true;
        pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNKNOWN;
        double freqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesFrequencyOcGetFrequencyTarget(handle, &freqTarget));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetVoltageTargetThenVerifyCallSucceeds) {
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double voltageTarget = 0.0, voltageOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetVoltageTarget(handle, &voltageTarget, &voltageOffset));
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockVoltageTarget[domainIndex]), voltageTarget);
        EXPECT_DOUBLE_EQ(static_cast<double>(pKmdSysManager->mockVoltageOffset[domainIndex]), voltageOffset);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetVoltageTargetAndRequestMultipleFailsThenFailureIsReturned) {
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->requestMultipleSizeDiff = false;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_INVALID_SIZE;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double voltageTarget = 0.0, voltageOffset = 0.0;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, zesFrequencyOcGetVoltageTarget(handle, &voltageTarget, &voltageOffset));
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleWhenCallingzesFrequencyOcGetModeThenVerifyCallSucceeds) {
    pKmdSysManager->allowSetCalls = false;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &mode));
        EXPECT_DOUBLE_EQ(ZES_OC_MODE_INTERPOLATIVE, mode);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToFalseWhenCallingzesFrequencyOcSetFrequencyTargetThenVerifyCallFails) {
    pKmdSysManager->allowSetCalls = false;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 1400.0;
        EXPECT_NE(ZE_RESULT_SUCCESS, zesFrequencyOcSetFrequencyTarget(handle, freqTarget));
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToFalseWhenCallingzesFrequencyOcSetVoltageTargetThenVerifyCallFails) {
    pKmdSysManager->allowSetCalls = false;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double voltageTarget = 1040.0, voltageOffset = 20.0;
        EXPECT_NE(ZE_RESULT_SUCCESS, zesFrequencyOcSetVoltageTarget(handle, voltageTarget, voltageOffset));
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToFalseWhenCallingzesFrequencyOcSetModeThenVerifyCallFails) {
    pKmdSysManager->allowSetCalls = false;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        zes_oc_mode_t mode = ZES_OC_MODE_OVERRIDE;
        EXPECT_NE(ZE_RESULT_SUCCESS, zesFrequencyOcSetMode(handle, mode));
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToTrueWhenCallingzesFrequencyOcSetFrequencyTargetThenVerifyCallSucceed) {
    pKmdSysManager->allowSetCalls = true;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double freqTarget = 1400.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetFrequencyTarget(handle, freqTarget));
        double newFreqTarget = 0.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetFrequencyTarget(handle, &newFreqTarget));
        EXPECT_DOUBLE_EQ(newFreqTarget, freqTarget);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToTrueWhenCallingzesFrequencyOcSetVoltageTargetThenVerifyCallSucceed) {
    pKmdSysManager->allowSetCalls = true;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double voltageTarget = 1040.0, voltageOffset = 20.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetVoltageTarget(handle, voltageTarget, voltageOffset));
        double newVoltageTarget = 1040.0, newVoltageOffset = 20.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetVoltageTarget(handle, &newVoltageTarget, &newVoltageOffset));
        EXPECT_DOUBLE_EQ(voltageTarget, newVoltageTarget);
        EXPECT_DOUBLE_EQ(voltageOffset, newVoltageOffset);
        domainIndex++;
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToTrueWhenCallingzesFrequencyOcSetIccMaxTargetThenVerifyCallSucceed) {
    pKmdSysManager->allowSetCalls = true;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double setIccMax = 1050.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetIccMax(handle, setIccMax));
        double newSetIccMax = 0.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetIccMax(handle, &newSetIccMax));
        EXPECT_DOUBLE_EQ(setIccMax, newSetIccMax);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToTrueWhenCallingzesFrequencyOcSetTjMaxTargetThenVerifyCallSucceed) {
    pKmdSysManager->allowSetCalls = true;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {
        double setTjMax = 1050.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetTjMax(handle, setTjMax));
        double newSetTjMax = 0.0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetTjMax(handle, &newSetTjMax));
        EXPECT_DOUBLE_EQ(newSetTjMax, setTjMax);
    }
}

TEST_F(SysmanDeviceFrequencyFixture, GivenValidFrequencyHandleAllowSetCallsToTrueWhenCallingzesFrequencyOcSetModeThenVerifyCallSucceed) {
    pKmdSysManager->allowSetCalls = true;
    uint32_t domainIndex = 0;
    auto handles = get_frequency_handles(frequencyHandleComponentCount);
    for (auto handle : handles) {

        zes_oc_mode_t mode = ZES_OC_MODE_INTERPOLATIVE;
        zes_oc_mode_t newmode;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetMode(handle, mode));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &newmode));
        EXPECT_EQ(newmode, ZES_OC_MODE_INTERPOLATIVE);

        mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetMode(handle, mode));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &newmode));
        EXPECT_EQ(newmode, ZES_OC_MODE_INTERPOLATIVE);

        mode = ZES_OC_MODE_FIXED;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, zesFrequencyOcSetMode(handle, mode));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &newmode));
        EXPECT_EQ(newmode, ZES_OC_MODE_INTERPOLATIVE);

        mode = ZES_OC_MODE_OFF;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetMode(handle, mode));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &newmode));
        EXPECT_EQ(newmode, ZES_OC_MODE_INTERPOLATIVE);

        mode = ZES_OC_MODE_OVERRIDE;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcSetMode(handle, mode));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFrequencyOcGetMode(handle, &newmode));
        EXPECT_EQ(newmode, ZES_OC_MODE_OVERRIDE);

        domainIndex++;
    }
}

} // namespace ult
} // namespace L0
