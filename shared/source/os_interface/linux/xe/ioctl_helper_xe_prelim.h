/*
 * Copyright (C) 2024 Intel Corporation
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
};

} // namespace NEO
