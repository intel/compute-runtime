/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

#include <algorithm>

#define RETURN_ME(X) return X

namespace NEO {

uint64_t getClosestSamplingRate(uint64_t requestedRate, uint64_t *samplingRates, uint64_t size) {
    uint64_t samplingUnit = 0;

    for (uint64_t index = 0; index < size; index++) {
        // Need to find the sampling rate which is closest to the requested rate
        if (samplingRates[index] >= requestedRate) {
            if (index == 0) {
                // The requested sampling rate is smaller than all supported sampling rates or equal to the 1st sampling rate. Pick the smallest supported rate and exit.
                samplingUnit = samplingRates[index];
                break;
            }
            uint64_t oneLowerIndex = index - 1;
            uint64_t oneHigherIndex = index;

            uint64_t deltaWithOneLowerIndex = requestedRate - samplingRates[oneLowerIndex];
            uint64_t deltaWithOneHigherIndex = samplingRates[oneHigherIndex] - requestedRate;

            samplingUnit = deltaWithOneHigherIndex <= deltaWithOneLowerIndex ? samplingRates[oneHigherIndex] : samplingRates[oneLowerIndex];
            break;
        }
    }

    // Requested sampling rate is higher than the max supported rate. Select the highest sampling rate supported.
    if (samplingUnit == 0) {
        samplingUnit = samplingRates[size - 1];
    }
    return samplingUnit;
}

bool IoctlHelperXe::perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) {

    // Query Sampling rates
    drm_xe_query_eu_stall *euStallQueryData = nullptr;
    drm_xe_device_query euStallDeviceQuery = {};
    euStallDeviceQuery.extensions = 0;
    euStallDeviceQuery.query = DRM_XE_DEVICE_QUERY_EU_STALL;
    euStallDeviceQuery.size = 0;
    euStallDeviceQuery.data = 0;

    int ret = ioctl(DrmIoctl::perfQuery, &euStallDeviceQuery);
    if (ret != 0 || euStallDeviceQuery.size == 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (ret != 0), stderr,
                           "%s failed errno = %d | ret = %d \n", "DRM_IOCTL_XE_DEVICE_QUERY", errno, ret);
        return false;
    }

    std::vector<uint8_t> allocateMemory(euStallDeviceQuery.size);
    euStallQueryData = reinterpret_cast<drm_xe_query_eu_stall *>(allocateMemory.data());

    euStallDeviceQuery.data = reinterpret_cast<uint64_t>(euStallQueryData);
    ret = ioctl(DrmIoctl::perfQuery, &euStallDeviceQuery);
    if (ret != 0 || euStallQueryData->num_sampling_rates == 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (ret != 0), stderr,
                           "%s failed errno = %d | ret = %d \n", "DRM_IOCTL_XE_DEVICE_QUERY", errno, ret);
        return false;
    }

    // Select sampling rate.
    uint64_t gpuClockPeriodNs = CommonConstants::nsecPerSec / gpuTimeStampfrequency;
    uint64_t numberOfClocks = samplingPeriodNs / gpuClockPeriodNs;
    uint64_t samplingUnit = getClosestSamplingRate(numberOfClocks, reinterpret_cast<uint64_t *>(&euStallQueryData->sampling_rates), euStallQueryData->num_sampling_rates);
    samplingPeriodNs = static_cast<uint32_t>(samplingUnit) * static_cast<uint32_t>(gpuClockPeriodNs);

    // Populate the EU stall properties. The array will have property type and value in successive index.
    std::array<uint64_t, 6u> properties;
    properties[0] = drm_xe_eu_stall_property_id::DRM_XE_EU_STALL_PROP_SAMPLE_RATE;
    properties[1] = samplingUnit;
    properties[2] = drm_xe_eu_stall_property_id::DRM_XE_EU_STALL_PROP_WAIT_NUM_REPORTS;
    properties[3] = notifyNReports;
    properties[4] = drm_xe_eu_stall_property_id::DRM_XE_EU_STALL_PROP_GT_ID;
    properties[5] = engineInstance;

    // Call perf open ioctl.
    uint32_t numProperties = sizeof(properties) / (sizeof(properties[0]) * 2);
    std::vector<drm_xe_ext_set_property> extensionProperty(numProperties);
    drm_xe_observation_param observationParam = {};
    observationParam.extensions = 0;
    observationParam.observation_type = drm_xe_observation_type::DRM_XE_OBSERVATION_TYPE_EU_STALL;
    observationParam.observation_op = drm_xe_observation_op::DRM_XE_OBSERVATION_OP_STREAM_OPEN;
    observationParam.param = reinterpret_cast<uint64_t>(extensionProperty.data());

    // Chain the properties for perfOpen ioctl.
    drm_xe_ext_set_property *ext = extensionProperty.data();

    for (uint32_t i = 0; i < numProperties; i++) {
        ext->base.name = DRM_XE_EU_STALL_EXTENSION_SET_PROPERTY;
        ext->property = static_cast<uint32_t>(properties[i * 2]);
        ext->value = properties[(i * 2) + 1];
        ext++;
    }

    ext = extensionProperty.data();
    for (uint32_t j = 0; j < numProperties - 1; j++) {
        ext[j].base.next_extension = reinterpret_cast<uint64_t>(&ext[j + 1]);
    }

    *stream = ioctl(DrmIoctl::perfOpen, &observationParam);
    if (*stream < 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (*stream < 0), stderr,
                           "%s failed errno = %d | ret = %d \n", "DRM_IOCTL_XE_OBSERVATION", errno, *stream);
        return false;
    }

    auto flags = SysCalls::fcntl(*stream, F_GETFL);
    if (flags == -1) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "fcntl system call failed with return code %d\n", flags);
        return false;
    }
    auto status = SysCalls::fcntl(*stream, F_SETFL, flags | O_CLOEXEC | O_NONBLOCK);
    if (status != 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "fcntl system call failed with return code %d\n", status);
        return false;
    }

    ret = ioctl(*stream, DrmIoctl::perfEnable, 0);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (ret < 0), stderr,
                       "%s failed errno = %d | ret = %d \n", "DRM_XE_OBSERVATION_IOCTL_ENABLE", errno, ret);
    return (ret == 0) ? true : false;
}

bool IoctlHelperXe::perfDisableEuStallStream(int32_t *stream) {
    int disableStatus = ioctl(*stream, DrmIoctl::perfDisable, 0);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (disableStatus < 0), stderr,
                       "DRM_XE_OBSERVATION_IOCTL_DISABLE failed errno = %d | ret = %d \n", errno, disableStatus);

    int closeStatus = NEO::SysCalls::close(*stream);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (closeStatus < 0), stderr,
                       "close() failed errno = %d | ret = %d \n", errno, closeStatus);
    *stream = -1;

    return ((closeStatus == 0) && (disableStatus == 0)) ? true : false;
}

unsigned int IoctlHelperXe::getIoctlRequestValuePerf(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::perfOpen:
        RETURN_ME(DRM_IOCTL_XE_OBSERVATION);
    case DrmIoctl::perfEnable:
        RETURN_ME(DRM_XE_OBSERVATION_IOCTL_ENABLE);
    case DrmIoctl::perfDisable:
        RETURN_ME(DRM_XE_OBSERVATION_IOCTL_DISABLE);
    case DrmIoctl::perfQuery:
        RETURN_ME(DRM_IOCTL_XE_DEVICE_QUERY);

    default:
        return 0;
    }
}

int IoctlHelperXe::perfOpenIoctl(DrmIoctl request, void *arg) {
    auto ret = IoctlHelper::ioctl(request, arg);
    return ret;
}

bool IoctlHelperXe::isEuStallSupported() {
    return true;
}

} // namespace NEO