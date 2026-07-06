/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "os_metric_ip_sampling_imp_linux.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace L0 {

MetricIpSamplingLinuxImp::MetricIpSamplingLinuxImp(Device &device) : device(device) {}

ze_result_t MetricIpSamplingLinuxImp::startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) {

    const auto drm = device.getOsInterface()->getDriverModel()->as<NEO::Drm>();
    // gpuTimeStampfrequency will be in Hertz
    uint64_t gpuTimeStampfrequency = 0;
    ze_result_t ret = getMetricsTimerResolution(gpuTimeStampfrequency);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t euStallFdParameter = ioctlHelper->getEuStallFdParameter();
    auto engineInfo = drm->getEngineInfo();
    if (engineInfo == nullptr) {
        METRICS_LOG_ERR("%s", "Engine info is not available");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    auto classInstance = engineInfo->getEngineInstance(device.getPhysicalSubDeviceId(), aub_stream::ENGINE_CCS);
    if (classInstance == nullptr) {
        METRICS_LOG_ERR("%s", "Compute engine instance is not available");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (ze_result_t clampResult = clampNReports(notifyEveryNReports); clampResult != ZE_RESULT_SUCCESS) {
        return clampResult;
    }
    if (!ioctlHelper->perfOpenEuStallStream(euStallFdParameter, samplingPeriodNs, classInstance->engineInstance, notifyEveryNReports, gpuTimeStampfrequency, &stream)) {
        METRICS_LOG_ERR("%s", "Failed to open EU stall stream");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricIpSamplingLinuxImp::clampNReports(uint32_t &notifyEveryNReports) {
    const auto drm = device.getOsInterface()->getDriverModel()->as<NEO::Drm>();
    const int64_t perXeCoreMaxReports = drm->getIoctlHelper()->getEuStallMaxReportsPerXeCore();
    if (perXeCoreMaxReports < 0) {
        // The KMD exposes the capacity query but it failed; we cannot tell whether the request will
        // be accepted, and the static estimate does not match a query-capable xe KMD. Fail rather
        // than risk a stream open the KMD will reject.
        METRICS_LOG_ERR("%s", "Failed to query EU stall max reports per Xe-core");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (perXeCoreMaxReports == 0) {
        // No KMD capacity query (i915 or an older xe KMD): clamp to the static buffer estimate,
        // matching long-standing pre-query behaviour.
        notifyEveryNReports = clampToMaxSupportedReports(notifyEveryNReports);
        return ZE_RESULT_SUCCESS;
    }
    const auto &hwInfo = device.getNEODevice()->getHardwareInfo();
    // The EU-stall buffer is allocated per Xe-core, so device-wide capacity is the per-Xe-core
    // report count scaled by the number of Xe-cores. A Xe-core maps to a DualSubSlice, and on the
    // Linux topology path NEO sets DualSubSliceCount == SubSliceCount == subSliceCount
    // (drm_neo.cpp), so this holds on platforms where gmmlib leaves DualSubSliceCount unset. A live
    // device always has at least one enabled Xe-core; a zero count means the topology query is
    // broken, which is an unrecoverable invariant violation rather than something to paper over.
    UNRECOVERABLE_IF(hwInfo.gtSystemInfo.DualSubSliceCount == 0);
    const uint64_t maxReports = static_cast<uint64_t>(perXeCoreMaxReports) * hwInfo.gtSystemInfo.DualSubSliceCount;
    notifyEveryNReports = static_cast<uint32_t>(std::min<uint64_t>(notifyEveryNReports,
                                                                   std::min<uint64_t>(maxReports, std::numeric_limits<uint32_t>::max())));
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricIpSamplingLinuxImp::stopMeasurement() {
    const auto drm = device.getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto ioctlHelper = drm->getIoctlHelper();
    bool result = ioctlHelper->perfDisableEuStallStream(&stream);

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t MetricIpSamplingLinuxImp::readData(uint8_t *pRawData, size_t *pRawDataSize) {

    ssize_t ret = NEO::SysCalls::read(stream, pRawData, *pRawDataSize);
    if (ret >= 0) {
        *pRawDataSize = ret;
        return ZE_RESULT_SUCCESS;
    }

    *pRawDataSize = 0;

    // If read needs to try again, do not return error
    if (errno == EINTR || errno == EAGAIN || errno == EBUSY) {
        METRICS_LOG_INFO("read() needs to try again. errno = %d ", errno);
        return ZE_RESULT_SUCCESS;
    } else if (errno == EIO) {
        // on i915 EIO is not returned by KMD for any error conditions. Hence we can use this safely for both xe and i915.
        METRICS_LOG_INFO("Some data was lost during read(). errno = %d ", errno);
        return ZE_RESULT_WARNING_DROPPED_DATA;
    }

    METRICS_LOG_ERR("read() failed errno = %d | ret = %d", errno, ret);

    return ZE_RESULT_ERROR_UNKNOWN;
}

uint32_t MetricIpSamplingLinuxImp::getRequiredBufferSize(const uint32_t maxReportCount) {

    const auto hwInfo = device.getNEODevice()->getHardwareInfo();
    const auto maxSupportedReportCount = (maxDssBufferSize * hwInfo.gtSystemInfo.MaxDualSubSlicesSupported) / unitReportSize;
    return std::min(maxSupportedReportCount, maxReportCount) * getUnitReportSize();
}

uint32_t MetricIpSamplingLinuxImp::getUnitReportSize() {
    return unitReportSize;
}

bool MetricIpSamplingLinuxImp::isNReportsAvailable() {
    struct pollfd pollParams;
    memset(&pollParams, 0, sizeof(pollParams));

    DEBUG_BREAK_IF(stream == -1);

    pollParams.fd = stream;
    pollParams.revents = 0;
    pollParams.events = POLLIN;

    int32_t pollResult = NEO::SysCalls::poll(&pollParams, 1, 0u);
    if (pollResult < 0) {
        METRICS_LOG_ERR("poll() failed errno = %d | pollResult = %d", errno, pollResult);
    }

    if (pollResult > 0) {
        return true;
    }
    return false;
}

bool MetricIpSamplingLinuxImp::isOsSupportAvailable() {
    const auto drm = device.getOsInterface()->getDriverModel()->as<NEO::Drm>();
    auto ioctlHelper = drm->getIoctlHelper();
    return ioctlHelper->isEuStallSupported();
}

ze_result_t MetricIpSamplingLinuxImp::getMetricsTimerResolution(uint64_t &timerResolution) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    const auto drm = device.getOsInterface()->getDriverModel()->as<NEO::Drm>();
    int32_t gpuTimeStampfrequency = 0;
    int32_t ret = drm->getTimestampFrequency(gpuTimeStampfrequency);
    if (ret < 0 || gpuTimeStampfrequency == 0) {
        timerResolution = 0;
        result = ZE_RESULT_ERROR_UNKNOWN;
        METRICS_LOG_ERR("getTimestampFrequency() failed errno = %d | ret = %d", errno, ret);
    } else {
        timerResolution = static_cast<uint64_t>(gpuTimeStampfrequency);
    }

    return result;
}

std::unique_ptr<MetricIpSamplingOsInterface> MetricIpSamplingOsInterface::create(Device &device) {
    return std::make_unique<MetricIpSamplingLinuxImp>(device);
}

} // namespace L0
