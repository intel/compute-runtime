/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include "igfxfmid.h"

#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace L0 {
namespace ult {

class MetricIpSamplingWindowsFixtureXe2 : public DeviceFixture,
                                          public ::testing::Test {
  public:
    void SetUp() override {
        DeviceFixture::setUp();

        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->osInterface = std::make_unique<NEO::OSInterface>();
        auto &osInterface = *device->getOsInterface();
        wddm = new NEO::WddmMock(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        wddm->init();
        osInterface.setDriverModel(std::unique_ptr<DriverModel>(wddm));
        metricIpSamplingOsInterface = MetricIpSamplingOsInterface::create(static_cast<L0::Device &>(*device));
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
    NEO::WddmMock *wddm;
    std::unique_ptr<MetricIpSamplingOsInterface> metricIpSamplingOsInterface = nullptr;
};

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenCorrectArgumentsWhenStartMeasurementIsCalledThenReturnSuccess, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 1u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        return true;
    });
    constexpr uint32_t samplingGranularity = 251u;
    constexpr uint32_t gpuClockPeriodNs = 1000000000ull;
    constexpr uint32_t samplingUnit = 1;
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(samplingPeriodNs, samplingGranularity * samplingUnit * gpuClockPeriodNs);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenCorrectArgumentsWhenStartMeasurementIsCalledThenCheckIfProperNotifyValueIsSentAndReturnSuccess, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 1u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        if (minBufferSize == 512 * MemoryConstants::kiloByte) {
            return true;
        }
        return false;
    });
    constexpr uint32_t samplingGranularity = 251u;
    constexpr uint32_t gpuClockPeriodNs = 1000000000ull;
    constexpr uint32_t samplingUnit = 1;
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(samplingPeriodNs, samplingGranularity * samplingUnit * gpuClockPeriodNs);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenGetTimestampFrequencyReturnsFrequencyEqualZeroWhenStartMeasurementIsCalledThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 0u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        return true;
    });
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfOpenEuStallStreamFailsWhenStartMeasurementIsCalledThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 1u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        return false;
    });
    uint32_t notifyEveryNReports = 0, samplingPeriodNs = 10000;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfDisableEuStallStreamSucceedsWhenStopMeasurementIsCalledThenReturnSuccess, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pPerfDisableEuStallStream)> mockPerfDisableEuStallStream(&NEO::pPerfDisableEuStallStream, []() -> bool {
        return true;
    });
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfDisableEuStallStreamFailsWhenStopMeasurementIsCalledThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pPerfDisableEuStallStream)> mockPerfDisableEuStallStream(&NEO::pPerfDisableEuStallStream, []() -> bool {
        return false;
    });
    EXPECT_EQ(metricIpSamplingOsInterface->stopMeasurement(), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenReadSucceedsWhenReadDataIsCalledThenReturnSuccess, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        return true;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfReadEuStallStreamFailsWhenReadDataIsCalledThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        return false;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfReadEuStallStreamOverflowsWhenReadDataIsCalledThenReturnCorrectOverflowStatus, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        *pOutRetCode = 13; // GTDI_RET_BUFFER_OVERFLOW
        return true;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_WARNING_DROPPED_DATA);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfReadEuStallStreamOverflowsWhenReadDataIsCalledTwiceThenCorrectReturnStatusAreReturned, IsXe2HpgCore) {
    static bool overflowRetuned = false;
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        if (!overflowRetuned) {
            overflowRetuned = true;
            *pOutRetCode = 13; // GTDI_RET_BUFFER_OVERFLOW
        }
        return true;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_WARNING_DROPPED_DATA);
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_SUCCESS);
    overflowRetuned = false;
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, givenPerfReadEuStallStreamOverflowsWhenReadDataIsCalledTwiceAndReadDataFailsOnScondCallThenCorrectReturnStatusAreReturned, IsXe2HpgCore) {
    static bool overflowRetuned = false;
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        if (!overflowRetuned) {
            overflowRetuned = true;
            *pOutRetCode = 13; // GTDI_RET_BUFFER_OVERFLOW
            return true;
        }
        return false;
    });
    uint8_t pRawData = 0u;
    size_t pRawDataSize = 0;
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_WARNING_DROPPED_DATA);
    EXPECT_EQ(metricIpSamplingOsInterface->readData(&pRawData, &pRawDataSize), ZE_RESULT_ERROR_UNKNOWN);
    overflowRetuned = false;
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, WhenGetRequiredBufferSizeIsCalledThenCorrectSizeIsReturned, IsXe2HpgCore) {
    constexpr uint32_t unitReportSize = 64;
    EXPECT_EQ(metricIpSamplingOsInterface->getRequiredBufferSize(10), unitReportSize * 10);
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, WhenisNReportsAvailableIsCalledAndEnoughReportsAreNotAvailableThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 1u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        return true;
    });
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        *pRawDataSize = 64u;
        return true;
    });
    constexpr uint32_t samplingGranularity = 251u;
    constexpr uint32_t gpuClockPeriodNs = 1000000000ull;
    constexpr uint32_t samplingUnit = 1;
    uint32_t notifyEveryNReports = 2, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(samplingPeriodNs, samplingGranularity * samplingUnit * gpuClockPeriodNs);
    EXPECT_FALSE(metricIpSamplingOsInterface->isNReportsAvailable());
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, WhenisNReportsAvailableIsCalledAndEnoughReportsAreAvailableThenReturnSuccess, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pGetTimestampFrequency)> mockGetTimestampFrequency(&NEO::pGetTimestampFrequency, []() -> uint32_t {
        return 1u;
    });
    VariableBackup<decltype(NEO::pPerfOpenEuStallStream)> mockPerfOpenEuStallStream(&NEO::pPerfOpenEuStallStream, [](uint32_t sampleRate, uint32_t minBufferSize) -> bool {
        return true;
    });
    VariableBackup<decltype(NEO::pPerfReadEuStallStream)> mockPerfReadEuStallStream(&NEO::pPerfReadEuStallStream, [](uint8_t *pRawData, size_t *pRawDataSize, uint32_t *pOutRetCode) -> bool {
        *pRawDataSize = 192u;
        return true;
    });
    constexpr uint32_t samplingGranularity = 251u;
    constexpr uint32_t gpuClockPeriodNs = 1000000000ull;
    constexpr uint32_t samplingUnit = 1;
    uint32_t notifyEveryNReports = 2, samplingPeriodNs = samplingGranularity * samplingUnit * gpuClockPeriodNs;
    EXPECT_EQ(metricIpSamplingOsInterface->startMeasurement(notifyEveryNReports, samplingPeriodNs), ZE_RESULT_SUCCESS);
    EXPECT_EQ(samplingPeriodNs, samplingGranularity * samplingUnit * gpuClockPeriodNs);
    EXPECT_TRUE(metricIpSamplingOsInterface->isNReportsAvailable());
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, WhenisNReportsAvailableIsCalledAndPerfReadEuStallStreamFailsThenReturnFailure, IsXe2HpgCore) {
    VariableBackup<decltype(NEO::pCallEscape)> mockCallEscape(&NEO::pCallEscape, [](D3DKMT_ESCAPE &escapeCommand) -> NTSTATUS {
        return -1;
    });
    EXPECT_FALSE(metricIpSamplingOsInterface->isNReportsAvailable());
}

HWTEST2_F(MetricIpSamplingWindowsFixtureXe2, GivenSupportedProductFamilyIsUsedWhenIsDependencyAvailableIsCalledThenReturnSuccess, IsXe2HpgCore) {
    auto hwInfo = neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.eProductFamily = productFamily;

    EXPECT_TRUE(metricIpSamplingOsInterface->isDependencyAvailable());
}

} // namespace ult
} // namespace L0
