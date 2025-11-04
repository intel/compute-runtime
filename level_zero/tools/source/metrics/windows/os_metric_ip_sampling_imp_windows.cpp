/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {

class MetricIpSamplingWindowsImp : public MetricIpSamplingOsInterface {
  public:
    MetricIpSamplingWindowsImp(Device &device);
    ~MetricIpSamplingWindowsImp() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override;
    ze_result_t stopMeasurement() override;
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override;
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override;
    uint32_t getUnitReportSize() override;
    bool isNReportsAvailable() override;
    bool isDependencyAvailable() override;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;

  private:
    Device &device;
    uint32_t notifyEveryNReports = 0u;
    ze_result_t getNearestSupportedSamplingUnit(uint32_t &samplingPeriodNs, uint32_t &samplingRate);
};

std::unique_ptr<MetricIpSamplingOsInterface> MetricIpSamplingOsInterface::create(Device &device) {
    return std::make_unique<MetricIpSamplingWindowsImp>(device);
}

MetricIpSamplingWindowsImp::MetricIpSamplingWindowsImp(Device &device) : device(device) {}

ze_result_t MetricIpSamplingWindowsImp::startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) {
    const auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();

    uint32_t samplingUnit = 0u;
    if (getNearestSupportedSamplingUnit(samplingPeriodNs, samplingUnit) != ZE_RESULT_SUCCESS) {
        METRICS_LOG_ERR("wddm getNearestSupportedSamplingUnit() call failed.");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    notifyEveryNReports = std::max(notifyEveryNReports, 1u);
    // Set the maximum DSS buffer size supported which is 512KB.
    uint32_t minBufferSize = 512 * MemoryConstants::kiloByte;

    if (!wddm->perfOpenEuStallStream(samplingUnit, minBufferSize)) {
        METRICS_LOG_ERR("wddm perfOpenEuStallStream() call failed.");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    this->notifyEveryNReports = notifyEveryNReports;
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricIpSamplingWindowsImp::readData(uint8_t *pRawData, size_t *pRawDataSize) {
    const auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();
    bool result = wddm->perfReadEuStallStream(pRawData, pRawDataSize);

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t MetricIpSamplingWindowsImp::stopMeasurement() {
    const auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();
    bool result = wddm->perfDisableEuStallStream();

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

uint32_t MetricIpSamplingWindowsImp::getRequiredBufferSize(const uint32_t maxReportCount) {

    const auto hwInfo = device.getNEODevice()->getHardwareInfo();
    const auto maxSupportedReportCount = (maxDssBufferSize * hwInfo.gtSystemInfo.MaxSubSlicesSupported) / unitReportSize;
    return std::min(maxSupportedReportCount, maxReportCount) * getUnitReportSize();
}

uint32_t MetricIpSamplingWindowsImp::getUnitReportSize() {
    return unitReportSize;
}

bool MetricIpSamplingWindowsImp::isNReportsAvailable() {
    size_t bytesAvailable = 0u;
    const auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();
    bool result = wddm->perfReadEuStallStream(nullptr, &bytesAvailable);
    if (!result) {
        METRICS_LOG_ERR("wddm perfReadEuStallStream() call failed.");
        return false;
    }
    return (bytesAvailable / unitReportSize) >= notifyEveryNReports ? true : false;
}

bool MetricIpSamplingWindowsImp::isDependencyAvailable() {

    const auto &hardwareInfo = device.getNEODevice()->getHardwareInfo();
    const auto &productHelper = device.getNEODevice()->getProductHelper();

    return productHelper.isIpSamplingSupported(hardwareInfo) ? true : false;
}

ze_result_t MetricIpSamplingWindowsImp::getMetricsTimerResolution(uint64_t &timerResolution) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    const auto wddm = device.getOsInterface()->getDriverModel()->as<NEO::Wddm>();
    uint32_t gpuTimeStampfrequency = 0;
    gpuTimeStampfrequency = wddm->getTimestampFrequency();
    if (gpuTimeStampfrequency == 0) {
        timerResolution = 0;
        result = ZE_RESULT_ERROR_UNKNOWN;
        METRICS_LOG_ERR("getTimestampFrequency() failed errno = %d | gpuTimeStampfrequency = %d ",
                        errno,
                        gpuTimeStampfrequency);

    } else {
        timerResolution = static_cast<uint64_t>(gpuTimeStampfrequency);
    }

    return result;
}

ze_result_t MetricIpSamplingWindowsImp::getNearestSupportedSamplingUnit(uint32_t &samplingPeriodNs, uint32_t &samplingUnit) {

    static constexpr uint32_t samplingClockGranularity = 251u;
    static constexpr uint32_t minSamplingUnit = 1u;
    static constexpr uint32_t maxSamplingUnit = 7u;

    uint64_t gpuTimeStampfrequency = 0;
    ze_result_t ret = getMetricsTimerResolution(gpuTimeStampfrequency);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    uint64_t gpuClockPeriodNs = CommonConstants::nsecPerSec / gpuTimeStampfrequency;
    UNRECOVERABLE_IF(gpuClockPeriodNs == 0);
    uint64_t numberOfClocks = samplingPeriodNs / gpuClockPeriodNs;

    samplingUnit = std::clamp(static_cast<uint32_t>(numberOfClocks / samplingClockGranularity), minSamplingUnit, maxSamplingUnit);
    samplingPeriodNs = samplingUnit * samplingClockGranularity * static_cast<uint32_t>(gpuClockPeriodNs);
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
