/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing_factory.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_sharing_defines.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_sharing_functions.h"

namespace NEO {
namespace LEO {

const uint32_t VASharingFunctions::sharingId = SharingType::VA_SHARING;

template VASharingFunctions *Context::getSharing<VASharingFunctions>();

} // namespace LEO
} // namespace NEO
