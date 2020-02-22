/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.inl"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/source/sharings/va/va_sharing_defines.h"
#include "opencl/source/sharings/va/va_sharing_functions.h"

namespace NEO {
const uint32_t VASharingFunctions::sharingId = SharingType::VA_SHARING;

template VASharingFunctions *Context::getSharing<VASharingFunctions>();
} // namespace NEO
