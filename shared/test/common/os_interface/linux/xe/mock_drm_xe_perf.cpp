/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/xe/mock_drm_xe_perf.h"

#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"

std::unique_ptr<DrmMockXePerf> DrmMockXePerf::create(RootDeviceEnvironment &rootDeviceEnvironment) {
    auto drm = std::unique_ptr<DrmMockXePerf>(new DrmMockXePerf{rootDeviceEnvironment});
    drm->initInstance();

    drm->ioctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);

    return drm;
}

int DrmMockXePerf::ioctl(DrmIoctl request, void *arg) {
    int ret = -1;
    ioctlCalled = true;

    if (forceIoctlAnswer) {
        return getIoctlAnswer();
    }

    switch (request) {
    case DrmIoctl::perfQuery: {
        perfQueryCallCount++;
        if (failPerfQueryOnCall && (perfQueryCallCount >= failPerfQueryOnCall)) {
            return -1;
        }
        drm_xe_device_query *euStallDeviceQuery = static_cast<drm_xe_device_query *>(arg);
        uint64_t samplingRates[] = {251, 502};
        if (euStallDeviceQuery->size == 0) {
            if (querySizeZero) {
                euStallDeviceQuery->size = 0;
                return 0;
            } else {
                uint32_t sizeNeeded = sizeof(drm_xe_query_eu_stall) + sizeof(samplingRates);
                euStallDeviceQuery->size = sizeNeeded;
            }
        } else {
            drm_xe_query_eu_stall *euStallQueryData = reinterpret_cast<drm_xe_query_eu_stall *>(euStallDeviceQuery->data);
            if (numSamplingRateCountZero) {
                euStallQueryData->num_sampling_rates = 0;
                return 0;
            }
            euStallQueryData->num_sampling_rates = 2;
            memcpy_s(reinterpret_cast<void *>(euStallQueryData->sampling_rates), sizeof(samplingRates), &samplingRates, sizeof(samplingRates));
        }
        ret = 0;
    } break;
    default:
        return DrmMockXe::ioctl(request, arg);
    }

    return ret;
}
