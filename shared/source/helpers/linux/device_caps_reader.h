/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_caps_reader.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

class DeviceCapsReaderDrm : public DeviceCapsReader {
  public:
    DeviceCapsReaderDrm(std::vector<uint32_t> &&caps) : caps(std::move(caps)) {}
    static std::unique_ptr<DeviceCapsReaderDrm> create(const Drm &drm) {
        auto ioctlHelper = drm.getIoctlHelper();
        auto caps = ioctlHelper->queryDeviceCaps();
        if (!caps) {
            return nullptr;
        }

        return std::make_unique<DeviceCapsReaderDrm>(std::move(*caps));
    }

    uint32_t operator[](size_t offsetDw) const override {
        if (offsetDw >= caps.size()) {
            DEBUG_BREAK_IF(true);
            return 0;
        }

        return caps[offsetDw];
    }

  protected:
    std::vector<uint32_t> caps;
};

} // namespace NEO
