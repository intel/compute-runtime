/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/get_info.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/core/source/memory/internal_mem_alloc_ext.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

void *CL_API_CALL clSVMAlloc(cl_context context,
                             cl_svm_mem_flags flags,
                             size_t size,
                             cl_uint alignment) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return nullptr;
    }

    return clSharedMemAllocINTEL(context, nullptr, nullptr, size, alignment, nullptr);
}

void CL_API_CALL clSVMFree(cl_context context,
                           void *svmPointer) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return;
    }

    clMemFreeINTEL(context, svmPointer);
}

CL_API_ENTRY void *CL_API_CALL clHostMemAllocINTEL(
    cl_context context,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    NEO::LEO::MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if (!NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                   NEO::LEO::ClMemoryPropertiesHelper::ObjType::unknown,
                                                                   *NEO::LEO::castToObject<NEO::LEO::Context>(context))) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};

    if (memoryProperties.allocFlags.allocWriteCombined) {
        hostDesc.flags |= ZE_HOST_MEM_ALLOC_FLAG_BIAS_WRITE_COMBINED;
    }

    void *ptr = nullptr;
    ze_result_t ret = zeMemAllocHost(pContext->getL0ContextHandle(), &hostDesc, size, alignment, &ptr);
    err.set(L0ToClResultMapper(ret));
    return ptr;
}

CL_API_ENTRY void *CL_API_CALL clDeviceMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext, pDevice] = NEO::LEO::validateAndCast(std::make_tuple(context, device));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    NEO::LEO::MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if (!NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                   NEO::LEO::ClMemoryPropertiesHelper::ObjType::unknown,
                                                                   *NEO::LEO::castToObject<NEO::LEO::Context>(context))) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    ze_device_mem_alloc_desc_t deviceDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
    L0::ze_write_combined_mem_alloc_ext_desc_t writeCombinedDesc{};

    if (memoryProperties.allocFlags.allocWriteCombined) {
        deviceDesc.pNext = &writeCombinedDesc;
    }

    void *ptr = nullptr;
    ze_result_t ret = zeMemAllocDevice(pContext->getL0ContextHandle(), &deviceDesc, size, alignment, NEO::LEO::ConvertTo::zeDeviceHandle(device), &ptr);
    err.set(L0ToClResultMapper(ret));
    return ptr;
}

CL_API_ENTRY void *CL_API_CALL clSharedMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    NEO::LEO::MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if (!NEO::LEO::ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                   NEO::LEO::ClMemoryPropertiesHelper::ObjType::unknown,
                                                                   *NEO::LEO::castToObject<NEO::LEO::Context>(context))) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};
    ze_device_mem_alloc_desc_t deviceDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};

    if (memoryProperties.allocFlags.allocWriteCombined) {
        hostDesc.flags |= ZE_HOST_MEM_ALLOC_FLAG_BIAS_WRITE_COMBINED;
    }
    if (memoryProperties.allocFlags.usmInitialPlacementGpu) {
        deviceDesc.flags |= ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT;
    }
    if (memoryProperties.allocFlags.usmInitialPlacementCpu) {
        hostDesc.flags |= ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT;
    }

    void *ptr = nullptr;
    ze_result_t ret = zeMemAllocShared(pContext->getL0ContextHandle(), &deviceDesc, &hostDesc, size, alignment, device ? NEO::LEO::castToObject<NEO::LEO::ClDevice>(device)->getL0Handle() : nullptr, &ptr);
    err.set(L0ToClResultMapper(ret));
    return ptr;
}

CL_API_ENTRY cl_int CL_API_CALL clMemFreeCommon(cl_context context,
                                                void *ptr,
                                                bool blocking) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (!ptr) {
        return CL_SUCCESS;
    }

    ze_memory_free_ext_desc_t freeDesc{ZE_STRUCTURE_TYPE_MEMORY_FREE_EXT_DESC, nullptr, 0};
    freeDesc.freePolicy = blocking ? ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE : ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    return L0ToClResultMapper(zeMemFreeExt(pContext->getL0ContextHandle(), &freeDesc, ptr));
}

CL_API_ENTRY cl_int CL_API_CALL clMemFreeINTEL(
    cl_context context,
    void *ptr) {
    return clMemFreeCommon(context, ptr, false);
}

CL_API_ENTRY cl_int CL_API_CALL clMemBlockingFreeINTEL(
    cl_context context,
    void *ptr) {
    return clMemFreeCommon(context, ptr, true);
}

CL_API_ENTRY cl_int CL_API_CALL clGetMemAllocInfoINTEL(
    cl_context context,
    const void *ptr,
    cl_mem_info_intel paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {
    auto [retVal, pContext] = NEO::LEO::validateAndCast(std::make_tuple(context));
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return pContext->getMemAllocInfo(ptr, paramName, paramValueSize, paramValue, paramValueSizeRet);
}
