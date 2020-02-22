/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "api/cl_types.h"
#include "sharings/sharing.h"
#include "sharings/va/va_sharing_defines.h"
#include "sharings/va/va_sharing_functions.h"

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
