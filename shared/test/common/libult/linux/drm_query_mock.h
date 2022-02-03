/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

using namespace NEO;

class DrmQueryMock : public DrmMock {
  public:
    DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmQueryMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmQueryMock(RootDeviceEnvironment &rootDeviceEnvironment, const HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        context.hwInfo = rootDeviceEnvironment.getHardwareInfo();

        setupIoctlHelper(IGFX_UNKNOWN);
    }

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    DrmMockPrelimContext context{
        .hwInfo = nullptr,
        .rootDeviceEnvironment = rootDeviceEnvironment,
        .cacheInfo = getCacheInfo(),
        .failRetTopology = failRetTopology,
    };

    int handleRemainingRequests(unsigned long request, void *arg) override;
    virtual bool handleQueryItem(void *queryItem);
};
