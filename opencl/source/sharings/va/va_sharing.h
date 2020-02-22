/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/va/va_sharing_defines.h"
#include "opencl/source/sharings/va/va_sharing_functions.h"

namespace NEO {
class VASharing : public SharingHandler {
  public:
    VASharing(VASharingFunctions *sharingFunctions, VAImageID imageId)
        : sharingFunctions(sharingFunctions), imageId(imageId){};
    VASharingFunctions *peekFunctionsHandler() { return sharingFunctions; }

  protected:
    VASharingFunctions *sharingFunctions = nullptr;
    VAImageID imageId;
};
} // namespace NEO
