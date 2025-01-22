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

    bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) override;

    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;

  protected:
    void setContextProperties(const OsContextLinux &osContext, void *extProperties, uint32_t &extIndexInOut) override;
};

} // namespace NEO
