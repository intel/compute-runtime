/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/linux/os_time_linux.h"

namespace NEO {
class MockOSTimeLinux : public OSTimeLinux {
  public:
    MockOSTimeLinux(OSInterface *osInterface) : OSTimeLinux(osInterface){};
    void setResolutionFunc(resolutionFunc_t func) {
        this->resolutionFunc = func;
    }
    void setGetTimeFunc(getTimeFunc_t func) {
        this->getTimeFunc = func;
    }
    void updateDrm(Drm *drm) {
        osInterface->get()->setDrm(drm);
        pDrm = drm;
        timestampTypeDetect();
    }
    static std::unique_ptr<MockOSTimeLinux> create(OSInterface *osInterface) {
        return std::unique_ptr<MockOSTimeLinux>(new MockOSTimeLinux(osInterface));
    }
};
} // namespace NEO
