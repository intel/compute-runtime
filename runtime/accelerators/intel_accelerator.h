/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"

//------------------------------------------------------------------------------
// cl_intel_accelerator Class Stuff
//------------------------------------------------------------------------------

namespace NEO {

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
} // namespace NEO
