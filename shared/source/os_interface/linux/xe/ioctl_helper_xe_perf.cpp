/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

namespace NEO {

bool IoctlHelperXe::perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) {
    return false;
}

bool IoctlHelperXe::perfDisableEuStallStream(int32_t *stream) {
    return false;
}

unsigned int IoctlHelperXe::getIoctlRequestValuePerf(DrmIoctl ioctlRequest) const {
    return 0;
}

int IoctlHelperXe::perfOpenIoctl(DrmIoctl request, void *arg) {
    return 0;
}

bool IoctlHelperXe::isEuStallSupported() {
    return false;
}

} // namespace NEO
