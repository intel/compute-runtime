/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/sharings/sharing.h"
#include "runtime/sharings/va/va_sharing_defines.h"
#include "runtime/sharings/va/va_sharing_functions.h"

namespace OCLRT {
class VASharing : public SharingHandler {
  public:
    VASharing(VASharingFunctions *sharingFunctions, VAImageID imageId)
        : sharingFunctions(sharingFunctions), imageId(imageId){};
    VASharingFunctions *peekFunctionsHandler() { return sharingFunctions; }

  protected:
    VASharingFunctions *sharingFunctions = nullptr;
    VAImageID imageId;
};
} // namespace OCLRT
