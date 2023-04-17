/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <optional>

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
    int vmBind(const VmBindParams &vmBindParams) override {
        if (failBind.has_value())
            return *failBind ? -1 : 0;
        else
            return IoctlHelperPrelim20::vmBind(vmBindParams);
    }
    int vmUnbind(const VmBindParams &vmBindParams) override {
        if (failBind.has_value())
            return *failBind ? -1 : 0;
        else
            return IoctlHelperPrelim20::vmUnbind(vmBindParams);
    }
    bool isWaitBeforeBindRequired(bool bind) const override {
        if (waitBeforeBindRequired.has_value())
            return *waitBeforeBindRequired;
        else
            return IoctlHelperPrelim20::isWaitBeforeBindRequired(bind);
    }

    unsigned int ioctlRequestValue = 1234u;
    int drmParamValue = 1234;
    std::optional<bool> failBind{};
    std::optional<bool> waitBeforeBindRequired{};
};
} // namespace NEO
