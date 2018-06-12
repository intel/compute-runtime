/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/accelerators/intel_accelerator.h"

//------------------------------------------------------------------------------
// VmeAccelerator Class Stuff
//------------------------------------------------------------------------------

namespace OCLRT {

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
} // namespace OCLRT
