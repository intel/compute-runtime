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

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/context/context.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/get_info.h"

namespace OCLRT {

cl_int IntelAccelerator::getInfo(cl_accelerator_info_intel paramName,
                                 size_t paramValueSize,
                                 void *paramValue,
                                 size_t *paramValueSizeRet) const {
    cl_int result = CL_SUCCESS;
    size_t ret = 0;

    switch (paramName) {
    case CL_ACCELERATOR_DESCRIPTOR_INTEL: {
        ret = getDescriptorSize();
        result = ::getInfo(paramValue, paramValueSize, getDescriptor(), ret);
    }

    break;

    case CL_ACCELERATOR_REFERENCE_COUNT_INTEL: {
        auto v = getReference();

        ret = sizeof(cl_uint);
        result = ::getInfo(paramValue, paramValueSize, &v, ret);
    }

    break;

    case CL_ACCELERATOR_CONTEXT_INTEL: {
        ret = sizeof(cl_context);
        cl_context ctx = static_cast<cl_context>(pContext);
        result = ::getInfo(paramValue, paramValueSize, &ctx, ret);
    }

    break;

    case CL_ACCELERATOR_TYPE_INTEL: {
        auto v = getTypeId();
        ret = sizeof(cl_accelerator_type_intel);
        result = ::getInfo(paramValue, paramValueSize, &v, ret);
    }

    break;

    default:
        result = CL_INVALID_VALUE;
        break;
    }

    if (paramValueSizeRet) {
        *paramValueSizeRet = ret;
    }

    return result;
}
} // namespace OCLRT
