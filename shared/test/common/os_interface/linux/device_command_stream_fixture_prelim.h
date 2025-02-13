/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture_context.h"

struct DrmMockCustomPrelim : public DrmMockCustom {
    using Drm::cacheInfo;
    using Drm::ioctlHelper;
    using Drm::memoryInfo;

    static auto create(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto drm = std::unique_ptr<DrmMockCustomPrelim>(new DrmMockCustomPrelim{rootDeviceEnvironment});
        drm->reset();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        drm->ioctlExpected.contextCreate = static_cast<int>(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment).size());
        drm->ioctlExpected.contextDestroy = drm->ioctlExpected.contextCreate.load();

        drm->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*drm);

        drm->createVirtualMemoryAddressSpace(NEO::GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()));
        drm->isVmBindAvailable();
        drm->reset();

        return drm;
    }

    int ioctlExtra(DrmIoctl request, void *arg) override {
        return context.ioctlExtra(request, arg);
    }

    void execBufferExtensions(void *arg) override {
        return context.execBufferExtensions(arg);
    }

    bool checkResetStatus(OsContext &osContext) override {
        return false;
    }

    DrmMockCustomPrelimContext context{};

  protected:
    // Don't call directly, use the create() function
    DrmMockCustomPrelim(RootDeviceEnvironment &rootDeviceEnvironment)
        : DrmMockCustom(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {}
};
