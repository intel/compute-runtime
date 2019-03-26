/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/accelerators/intel_accelerator.h"

//------------------------------------------------------------------------------
// VmeAccelerator Class Stuff
//------------------------------------------------------------------------------

namespace NEO {

class Context;

class VmeAccelerator : public IntelAccelerator {
  public:
    static VmeAccelerator *create(Context *context,
                                  cl_accelerator_type_intel typeId,
                                  size_t descriptorSize,
                                  const void *descriptor,
                                  cl_int &result) {

        result = validateVmeArgs(context, typeId, descriptorSize, descriptor);
        VmeAccelerator *acc = nullptr;

        if (result == CL_SUCCESS) {
            acc = new VmeAccelerator(
                context,
                typeId,
                descriptorSize,
                descriptor);
        }

        return acc;
    }

  protected:
  private:
    VmeAccelerator(Context *context,
                   cl_accelerator_type_intel typeId,
                   size_t descriptorSize,
                   const void *descriptor) : IntelAccelerator(context,
                                                              typeId,
                                                              descriptorSize,
                                                              descriptor) {
    }
    static cl_int validateVmeArgs(Context *context,
                                  cl_accelerator_type_intel typeId,
                                  size_t descriptorSize,
                                  const void *descriptor);
};
} // namespace NEO
