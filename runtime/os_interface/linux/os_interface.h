/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/drm_residency_handler.h"
#include "runtime/os_interface/os_interface.h"

#include "drm_neo.h"

namespace NEO {
class Drm;

class OSInterface::OSInterfaceImpl {
  public:
    OSInterfaceImpl() {
        drm = nullptr;
        residencyInterface = std::make_unique<DrmResidencyHandler>();
    }
    Drm *getDrm() const {
        return drm;
    }
    void setDrm(Drm *drm) {
        this->drm = drm;
    }

    DrmResidencyHandler *getResidencyInterface() const {
        return residencyInterface.get();
    }

  protected:
    Drm *drm;
    std::unique_ptr<DrmResidencyHandler> residencyInterface;
};
} // namespace NEO
