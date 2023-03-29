/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {

class MockIoctlHelper : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override {
        return ioctlRequestValue;
    };

    int getDrmParamValue(DrmParam drmParam) const override {
        return drmParamValue;
    }

    unsigned int ioctlRequestValue = 1234u;
    int drmParamValue = 1234;
};
} // namespace NEO
