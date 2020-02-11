/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/os_interface.h"

#include <memory>

namespace NEO {
class Drm;

class OSInterface::OSInterfaceImpl {
  public:
    OSInterfaceImpl();
    ~OSInterfaceImpl();
    Drm *getDrm() const {
        return drm.get();
    }
    void setDrm(Drm *drm);

  protected:
    std::unique_ptr<Drm> drm;
};
} // namespace NEO
