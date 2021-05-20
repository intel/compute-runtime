/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_time.h"

#include <memory>

namespace NEO {

class OSTimeLinux : public OSTime {
  public:
    OSTimeLinux(OSInterface *osInterface, std::unique_ptr<DeviceTime> deviceTime);
    bool getCpuTime(uint64_t *timeStamp) override;
    double getHostTimerResolution() const override;
    uint64_t getCpuRawTimestamp() override;

  protected:
    typedef int (*resolutionFunc_t)(clockid_t, struct timespec *);
    typedef int (*getTimeFunc_t)(clockid_t, struct timespec *);
    resolutionFunc_t resolutionFunc;
    getTimeFunc_t getTimeFunc;
};

} // namespace NEO
