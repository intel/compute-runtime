/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_helpers.h"

#include "runtime/context/context.h"
#include "runtime/helpers/validators.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

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
    cl_mem buffer = Buffer::create(pContext, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL, size, hostPtr, diag);
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
