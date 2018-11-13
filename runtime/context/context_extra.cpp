/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"

namespace OCLRT {

cl_int Context::processExtraProperties(cl_context_properties propertyType, cl_context_properties propertyValue) {
    return CL_INVALID_PROPERTY;
}

} // namespace OCLRT
