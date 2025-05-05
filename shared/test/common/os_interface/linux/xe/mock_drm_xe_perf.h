/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"

using NEO::XeDrm::DrmMockXe;

struct DrmMockXePerf : public DrmMockXe {
    static std::unique_ptr<DrmMockXePerf> create(RootDeviceEnvironment &rootDeviceEnvironment);

    int ioctl(DrmIoctl request, void *arg) override;

    int32_t curFd = 0;
    bool querySizeZero = false;
    bool numSamplingRateCountZero = false;
    uint32_t perfQueryCallCount = 0;
    uint32_t failPerfQueryOnCall = 0;

  protected:
    // Don't call directly, use the create() function
    DrmMockXePerf(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockXe(rootDeviceEnvironment) {}
};