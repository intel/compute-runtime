/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/helpers/cl_gfx_core_helper.h"

namespace NEO {

struct ClGfxCoreHelperMock : public ClGfxCoreHelper {
    using ClGfxCoreHelper::makeDeviceIpVersion;
    using ClGfxCoreHelper::makeDeviceRevision;
};

} // namespace NEO
