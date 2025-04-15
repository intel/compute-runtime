/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {

class IoctlHelperXePrelim : public IoctlHelperXe {
  public:
    using IoctlHelperXe::IoctlHelperXe;

  protected:
    virtual bool isPrimaryContext(const OsContextLinux &osContext, uint32_t deviceIndex);
    virtual uint32_t getPrimaryContextId(const OsContextLinux &osContext, uint32_t deviceIndex, size_t contextIndex);
    void setContextProperties(const OsContextLinux &osContext, uint32_t deviceIndex, void *extProperties, uint32_t &extIndexInOut) override;
    bool isMediaGt(uint16_t gtType) const override;
};

} // namespace NEO
