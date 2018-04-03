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

#include "runtime/api/dispatch.h"
#include "runtime/api/api.h"
#include "runtime/helpers/get_info.h"
#include "runtime/context/context.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/mem_obj/image.h"

void OCLRT::MemObj::getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
#ifdef LIBVA
    case CL_MEM_VA_API_MEDIA_SURFACE_INTEL:
        peekSharingHandler()->getMemObjectInfo(*srcParamSize, *srcParam);
        break;
#endif
    default:
        break;
    }
}

void OCLRT::Image::getOsSpecificImageInfo(const cl_image_info &paramName, size_t *srcParamSize, void **srcParam) {
    switch (paramName) {
#ifdef LIBVA
    case CL_IMAGE_VA_API_PLANE_INTEL:
        *srcParamSize = sizeof(cl_uint);
        *srcParam = &mediaPlaneType;
        break;
#endif
    default:
        break;
    }
}

void *OCLRT::Context::getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize) {
    return nullptr;
}
