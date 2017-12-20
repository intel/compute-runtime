/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"

//------------------------------------------------------------------------------
// cl_intel_accelerator Class Stuff
//------------------------------------------------------------------------------

namespace OCLRT {

class Context;

typedef struct TagAcceleratorObjParams {
    cl_uint AcceleratorType;
    cl_uint AcceleratorFlags;
} OCLRT_ACCELERATOR_OBJECT_PARAMS, *POCLRT_ACCELERATOR_OBJECT_PARAMS;

template <>
struct OpenCLObjectMapper<_cl_accelerator_intel> {
    typedef class IntelAccelerator DerivedType;
};

class IntelAccelerator : public BaseObject<_cl_accelerator_intel> {
  public:
    IntelAccelerator(Context *context,
                     cl_accelerator_type_intel typeId,
                     size_t descriptorSize,
                     const void *descriptor) : pContext(context),
                                               typeId(typeId),
                                               descriptorSize(descriptorSize),
                                               pDescriptor(descriptor) {}

    IntelAccelerator() {}

    static const cl_ulong objectMagic = 0xC6D72FA2E81EA569ULL;

    cl_accelerator_type_intel getTypeId() const { return typeId; }

    size_t getDescriptorSize() const { return descriptorSize; }

    const void *getDescriptor() const { return pDescriptor; }

    cl_int getInfo(cl_accelerator_info_intel paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet) const;

  protected:
    Context *pContext = nullptr;
    const cl_accelerator_type_intel typeId = -1;
    const size_t descriptorSize = 0;
    const void *pDescriptor = nullptr;

  private:
};
} // namespace OCLRT
