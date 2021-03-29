/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_interface.h"

#include <memory>
#include <optional>
#include <string>

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

    bool isDebugAttachAvailable() const;

    static std::optional<std::string> getPciPath(int deviceFd);

  protected:
    std::unique_ptr<Drm> drm;
};
} // namespace NEO
