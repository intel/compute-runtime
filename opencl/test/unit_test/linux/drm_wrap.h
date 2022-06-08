/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/os_interface.h"

class DrmWrap : public NEO::Drm {
  public:
    using Drm::deviceId;
    using Drm::ioctlStatistics;
    using Drm::queryDeviceIdAndRevision;
    using Drm::revisionId;
    using Drm::virtualMemoryIds;
    static std::unique_ptr<DrmWrap> createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto hwDeviceIds = OSInterface::discoverDevices(rootDeviceEnvironment.executionEnvironment);
        if (!hwDeviceIds.empty()) {
            return std::unique_ptr<DrmWrap>{static_cast<DrmWrap *>(NEO::Drm::create(std::unique_ptr<HwDeviceIdDrm>(hwDeviceIds[0].release()->as<HwDeviceIdDrm>()), rootDeviceEnvironment))};
        }
        return nullptr;
    }
};

static_assert(sizeof(DrmWrap) == sizeof(NEO::Drm));
