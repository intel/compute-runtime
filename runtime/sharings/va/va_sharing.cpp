/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.inl"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/va/va_sharing_defines.h"
#include "runtime/sharings/va/va_sharing_functions.h"

namespace NEO {
const uint32_t VASharingFunctions::sharingId = SharingType::VA_SHARING;

template VASharingFunctions *Context::getSharing<VASharingFunctions>();
} // namespace NEO
