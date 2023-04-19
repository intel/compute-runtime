/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtpin_helpers.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/validators.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/api/api.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/mem_obj/buffer.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

using namespace gtpin;

namespace NEO {

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinCreateBuffer(context_handle_t context, uint32_t reqSize, resource_handle_t *pResource) {
    cl_int retVal = CL_SUCCESS;
    Context *pContext = castToObject<Context>(reinterpret_cast<cl_context>(context));
    if (isAnyNullptr(pContext, pResource)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }
    size_t size = alignUp(reqSize, MemoryConstants::cacheLineSize);
    auto clDevice = pContext->getDevice(0);
    auto &gtpinHelper = clDevice->getGTPinGfxCoreHelper();
    if (gtpinHelper.canUseSharedAllocation(clDevice->getHardwareInfo())) {
        void *unifiedMemorySharedAllocation = clSharedMemAllocINTEL(pContext, clDevice, 0, size, 0, &retVal);
        if (retVal != CL_SUCCESS) {
            return GTPIN_DI_ERROR_ALLOCATION_FAILED;
        }
        auto allocationsManager = pContext->getSVMAllocsManager();
        auto allocData = allocationsManager->getSVMAlloc(unifiedMemorySharedAllocation);
        *pResource = reinterpret_cast<resource_handle_t>(allocData);
    } else {
        void *hostPtr = pContext->getMemoryManager()->allocateSystemMemory(size, MemoryConstants::pageSize);
        cl_mem buffer = Buffer::create(pContext, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE | CL_MEM_FORCE_HOST_MEMORY_INTEL, size, hostPtr, retVal);
        if (retVal != CL_SUCCESS) {
            return GTPIN_DI_ERROR_ALLOCATION_FAILED;
        }
        *pResource = reinterpret_cast<resource_handle_t>(buffer);
    }
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinFreeBuffer(context_handle_t context, resource_handle_t resource) {
    Context *pContext = castToObject<Context>(reinterpret_cast<cl_context>(context));
    if (isAnyNullptr(pContext, resource)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }

    auto clDevice = pContext->getDevice(0);
    auto &gtpinHelper = clDevice->getGTPinGfxCoreHelper();
    if (gtpinHelper.canUseSharedAllocation(clDevice->getHardwareInfo())) {
        auto allocData = reinterpret_cast<SvmAllocationData *>(resource);
        auto graphicsAllocation = allocData->gpuAllocations.getGraphicsAllocation(clDevice->getRootDeviceIndex());
        clMemFreeINTEL(pContext, reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()));
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
    Context *pContext = castToObject<Context>(reinterpret_cast<cl_context>(context));
    if (isAnyNullptr(pContext, resource, pAddress)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }

    auto clDevice = pContext->getDevice(0);
    auto &gtpinHelper = clDevice->getGTPinGfxCoreHelper();
    if (gtpinHelper.canUseSharedAllocation(clDevice->getHardwareInfo())) {
        auto allocData = reinterpret_cast<SvmAllocationData *>(resource);
        auto graphicsAllocation = allocData->gpuAllocations.getGraphicsAllocation(clDevice->getRootDeviceIndex());
        *pAddress = reinterpret_cast<uint8_t *>(graphicsAllocation->getGpuAddress());

    } else {
        auto pMemObj = castToObject<MemObj>(resource);
        if (pMemObj == nullptr) {
            return GTPIN_DI_ERROR_INVALID_ARGUMENT;
        }
        *pAddress = reinterpret_cast<uint8_t *>(pMemObj->getHostPtr());
    }
    return GTPIN_DI_SUCCESS;
}

GTPIN_DI_STATUS GTPIN_DRIVER_CALLCONV gtpinUnmapBuffer(context_handle_t context, resource_handle_t resource) {
    Context *pContext = castToObject<Context>(reinterpret_cast<cl_context>(context));
    if (isAnyNullptr(pContext, resource)) {
        return GTPIN_DI_ERROR_INVALID_ARGUMENT;
    }

    auto clDevice = pContext->getDevice(0);
    auto &gtpinHelper = clDevice->getGTPinGfxCoreHelper();
    if (!gtpinHelper.canUseSharedAllocation(clDevice->getHardwareInfo())) {
        auto pMemObj = castToObject<MemObj>(resource);
        if (pMemObj == nullptr) {
            return GTPIN_DI_ERROR_INVALID_ARGUMENT;
        }
    }
    return GTPIN_DI_SUCCESS;
}
} // namespace NEO
