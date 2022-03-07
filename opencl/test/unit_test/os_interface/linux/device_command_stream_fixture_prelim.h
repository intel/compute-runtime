/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_context.h"

class DrmMockCustomPrelim : public DrmMockCustom {
  public:
    using Drm::cacheInfo;
    using Drm::ioctlHelper;
    using Drm::memoryInfo;

    DrmMockCustomPrelim(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockCustom(rootDeviceEnvironment) {
        setupIoctlHelper(IGFX_UNKNOWN);
    }

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    int ioctlExtra(unsigned long request, void *arg) override {
        return context.ioctlExtra(request, arg);
    }

    void execBufferExtensions(void *arg) override {
        return context.execBufferExtensions(arg);
    }

    DrmMockCustomPrelimContext context{};
};
