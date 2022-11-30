/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

struct ClGfxCoreHelperMock : public ClGfxCoreHelper {
    using ClGfxCoreHelper::makeDeviceIpVersion;
    using ClGfxCoreHelper::makeDeviceRevision;
};

} // namespace NEO
