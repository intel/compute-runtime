/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"

namespace NEO {

class DrmNullDevice : public Drm {

  public:
    int ioctl(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::Getparam || request == DrmIoctl::Query) {
            return Drm::ioctl(request, arg);
        } else if (request == DrmIoctl::RegRead) {
            auto *regArg = static_cast<RegisterRead *>(arg);
            // Handle only 36b timestamp
            if (regArg->offset == (REG_GLOBAL_TIMESTAMP_LDW | 1)) {
                gpuTimestamp += 1000;
                regArg->value = gpuTimestamp & 0x0000000FFFFFFFFF;
            } else if (regArg->offset == REG_GLOBAL_TIMESTAMP_LDW || regArg->offset == REG_GLOBAL_TIMESTAMP_UN) {
                return -1;
            }

            return 0;
        } else {
            return 0;
        }
    }

    DrmNullDevice(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::move(hwDeviceId), rootDeviceEnvironment){};

  protected:
    uint64_t gpuTimestamp = 0;
};
} // namespace NEO
