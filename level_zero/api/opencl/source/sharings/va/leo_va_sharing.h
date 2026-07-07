/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_sharing_defines.h"
#include "level_zero/api/opencl/source/sharings/va/leo_va_sharing_functions.h"

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
