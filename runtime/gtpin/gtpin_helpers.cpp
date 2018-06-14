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

#include "gtpin_ocl_interface.h"
#include "gtpin_helpers.h"
#include "CL/cl.h"
#include "runtime/context/context.h"
#include "runtime/helpers/validators.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"

using namespace gtpin;

namespace OCLRT {

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinCreateBuffer(context_handle_t context, uint32_t reqSize, resource_handle_t *pResource) {
    cl_int diag = CL_SUCCESS;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (pResource == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    size_t size = alignUp(reqSize, MemoryConstants::cacheLineSize);
    void *hostPtr = pContext->getMemoryManager()->allocateSystemMemory(size, MemoryConstants::pageSize);
    if (hostPtr == nullptr) {
        return GTPIN_DI_ERROR_ALLOCATION_FAILED;
    }
    cl_mem buffer = Buffer::create(pContext, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, size, hostPtr, diag);
    *pResource = (resource_handle_t)buffer;
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinFreeBuffer(context_handle_t context, resource_handle_t resource) {
    cl_mem buffer = (cl_mem)resource;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (buffer == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    auto pMemObj = castToObject<MemObj>(buffer);
    if (pMemObj == nullptr) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    alignedFree(pMemObj->getHostPtr());
    pMemObj->release();
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinMapBuffer(context_handle_t context, resource_handle_t resource, uint8_t **pAddress) {
    cl_mem buffer = (cl_mem)resource;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (buffer == nullptr) || (pAddress == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    auto pMemObj = castToObject<MemObj>(buffer);
    if (pMemObj == nullptr) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    *pAddress = (uint8_t *)pMemObj->getHostPtr();
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinUnmapBuffer(context_handle_t context, resource_handle_t resource) {
    cl_mem buffer = (cl_mem)resource;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (buffer == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    auto pMemObj = castToObject<MemObj>(buffer);
    if (pMemObj == nullptr) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    return GTPIN_DI_SUCCESS;
}
} // namespace OCLRT
