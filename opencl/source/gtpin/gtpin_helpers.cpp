/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_helpers.h"

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/api/api.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/mem_obj/buffer.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

using namespace gtpin;

namespace NEO {

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinCreateBuffer(context_handle_t context, uint32_t reqSize, resource_handle_t *pResource) {
    cl_int diag = CL_SUCCESS;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (pResource == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    size_t size = alignUp(reqSize, MemoryConstants::cacheLineSize);
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pContext->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
    if (gtpinHelper.canUseSharedAllocation(pContext->getDevice(0)->getHardwareInfo())) {
        void *unifiedMemorySharedAllocation = clSharedMemAllocINTEL(pContext, pContext->getDevice(0), 0, size, 0, &diag);
        auto allocationsManager = pContext->getSVMAllocsManager();
        auto graphicsAllocation = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
        *pResource = (resource_handle_t)graphicsAllocation;
    } else {
        void *hostPtr = pContext->getMemoryManager()->allocateSystemMemory(size, MemoryConstants::pageSize);
        if (hostPtr == nullptr) {
            return GTPIN_DI_ERROR_ALLOCATION_FAILED;
        }
        cl_mem buffer = Buffer::create(pContext, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_FORCE_HOST_MEMORY_INTEL, size, hostPtr, diag);
        *pResource = (resource_handle_t)buffer;
    }
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinFreeBuffer(context_handle_t context, resource_handle_t resource) {
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (resource == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pContext->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
    if (gtpinHelper.canUseSharedAllocation(pContext->getDevice(0)->getHardwareInfo())) {
        auto allocData = reinterpret_cast<SvmAllocationData *>(resource);
        clMemFreeINTEL(pContext, allocData->cpuAllocation->getUnderlyingBuffer());
    } else {
        auto pMemObj = castToObject<MemObj>(resource);
        if (pMemObj == nullptr) {
            return GTPIN_DI_ERROR_INVALID_ARGUMENT;
        }
        alignedFree(pMemObj->getHostPtr());
        pMemObj->release();
    }
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinMapBuffer(context_handle_t context, resource_handle_t resource, uint8_t **pAddress) {
    cl_mem buffer = (cl_mem)resource;
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (buffer == nullptr) || (pAddress == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pContext->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
    if (gtpinHelper.canUseSharedAllocation(pContext->getDevice(0)->getHardwareInfo())) {
        auto allocData = reinterpret_cast<SvmAllocationData *>(resource);
        *pAddress = reinterpret_cast<uint8_t *>(allocData->cpuAllocation->getUnderlyingBuffer());
    } else {
        auto pMemObj = castToObject<MemObj>(buffer);
        if (pMemObj == nullptr) {
            return GTPIN_DI_ERROR_INVALID_ARGUMENT;
        }
        *pAddress = reinterpret_cast<uint8_t *>(pMemObj->getHostPtr());
    }
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinUnmapBuffer(context_handle_t context, resource_handle_t resource) {
    Context *pContext = castToObject<Context>((cl_context)context);
    if ((pContext == nullptr) || (resource == nullptr)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pContext->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
    if (!gtpinHelper.canUseSharedAllocation(pContext->getDevice(0)->getHardwareInfo())) {
        auto pMemObj = castToObject<MemObj>(resource);
        if (pMemObj == nullptr) {
            return GTPIN_DI_ERROR_INVALID_ARGUMENT;
        }
    }
    return GTPIN_DI_SUCCESS;
}
} // namespace NEO
