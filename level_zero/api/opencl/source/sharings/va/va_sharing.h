/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/sharings/sharing.h"
#include "level_zero/api/opencl/source/sharings/va/va_sharing_defines.h"
#include "level_zero/api/opencl/source/sharings/va/va_sharing_functions.h"

namespace NEO {
namespace LEO {

class VASharing : public SharingHandler {
  public:
    VASharing(VASharingFunctions *sharingFunctions, VAImageID imageId)
        : sharingFunctions(sharingFunctions), imageId(imageId) {};
    VASharingFunctions *peekFunctionsHandler() { return sharingFunctions; }

  protected:
    VASharingFunctions *sharingFunctions = nullptr;
    VAImageID imageId;
};

} // namespace LEO
} // namespace NEO
