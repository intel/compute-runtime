/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

struct ClHwHelperMock : public ClHwHelper {
    using ClHwHelper::makeDeviceIpVersion;
    using ClHwHelper::makeDeviceRevision;
};

} // namespace NEO
