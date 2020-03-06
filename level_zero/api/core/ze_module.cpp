/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeModuleCreate(
    ze_device_handle_t hDevice,
    const ze_module_desc_t *desc,
    ze_module_handle_t *phModule,
    ze_module_build_log_handle_t *phBuildLog) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_MODULE_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createModule(desc, phModule, phBuildLog);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleDestroy(
    ze_module_handle_t hModule) {
    try {
        {
            if (nullptr == hModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Module::fromHandle(hModule)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogDestroy(
    ze_module_build_log_handle_t hModuleBuildLog) {
    try {
        {
            if (nullptr == hModuleBuildLog)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::moduleBuildLogDestroy(hModuleBuildLog);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleBuildLogGetString(
    ze_module_build_log_handle_t hModuleBuildLog,
    size_t *pSize,
    char *pBuildLog) {
    try {
        {
            if (nullptr == hModuleBuildLog)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pSize)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::ModuleBuildLog::fromHandle(hModuleBuildLog)->getString(pSize, pBuildLog);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleGetNativeBinary(
    ze_module_handle_t hModule,
    size_t *pSize,
    uint8_t *pModuleNativeBinary) {
    try {
        {
            if (nullptr == hModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pSize)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Module::fromHandle(hModule)->getNativeBinary(pSize, pModuleNativeBinary);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleGetGlobalPointer(
    ze_module_handle_t hModule,
    const char *pGlobalName,
    void **pptr) {
    try {
        {
            if (nullptr == hModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pGlobalName)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Module::fromHandle(hModule)->getGlobalPointer(pGlobalName, pptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleGetKernelNames(
    ze_module_handle_t hModule,
    uint32_t *pCount,
    const char **pNames) {
    if (nullptr == hModule) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (nullptr == pCount) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return L0::Module::fromHandle(hModule)->getKernelNames(pCount, pNames);
}

__zedllexport ze_result_t __zecall
zeKernelCreate(
    ze_module_handle_t hModule,
    const ze_kernel_desc_t *desc,
    ze_kernel_handle_t *phFunction) {
    try {
        {
            if (nullptr == hModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_KERNEL_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Module::fromHandle(hModule)->createKernel(desc, phFunction);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelDestroy(
    ze_kernel_handle_t hKernel) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hKernel)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeModuleGetFunctionPointer(
    ze_module_handle_t hModule,
    const char *pKernelName,
    void **pfnFunction) {
    try {
        {
            if (nullptr == hModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pKernelName)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pfnFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Module::fromHandle(hModule)->getFunctionPointer(pKernelName, pfnFunction);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelSetGroupSize(
    ze_kernel_handle_t hFunction,
    uint32_t groupSizeX,
    uint32_t groupSizeY,
    uint32_t groupSizeZ) {
    try {
        {
            if (nullptr == hFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hFunction)->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelSuggestGroupSize(
    ze_kernel_handle_t hFunction,
    uint32_t globalSizeX,
    uint32_t globalSizeY,
    uint32_t globalSizeZ,
    uint32_t *groupSizeX,
    uint32_t *groupSizeY,
    uint32_t *groupSizeZ) {
    try {
        {
            if (nullptr == hFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == groupSizeX)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == groupSizeY)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == groupSizeZ)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hFunction)->suggestGroupSize(globalSizeX, globalSizeY, globalSizeZ, groupSizeX, groupSizeY, groupSizeZ);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hKernel,
    uint32_t *totalGroupCount) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == totalGroupCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        (*totalGroupCount) = L0::Kernel::fromHandle(hKernel)->suggestMaxCooperativeGroupCount();
        return ZE_RESULT_SUCCESS;
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeFunctionSuggestMaxCooperativeGroupCount(
    ze_kernel_handle_t hFunction,
    uint32_t *groupCountX,
    uint32_t *groupCountY,
    uint32_t *groupCountZ);

__zedllexport ze_result_t __zecall
zeKernelSetArgumentValue(
    ze_kernel_handle_t hFunction,
    uint32_t argIndex,
    size_t argSize,
    const void *pArgValue) {
    try {
        {
            if (nullptr == hFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hFunction)->setArgumentValue(argIndex, argSize, pArgValue);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelSetAttribute(
    ze_kernel_handle_t hKernel,
    ze_kernel_attribute_t attr,
    uint32_t size,
    const void *pValue) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hKernel)->setAttribute(attr, size, pValue);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelGetAttribute(
    ze_kernel_handle_t hKernel,
    ze_kernel_attribute_t attr,
    uint32_t *pSize,
    void *pValue) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hKernel)->getAttribute(attr, pSize, pValue);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelSetIntermediateCacheConfig(
    ze_kernel_handle_t hKernel,
    ze_cache_config_t cacheConfig) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hKernel)->setIntermediateCacheConfig(cacheConfig);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeKernelGetProperties(
    ze_kernel_handle_t hKernel,
    ze_kernel_properties_t *pKernelProperties) {
    try {
        {
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pKernelProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Kernel::fromHandle(hKernel)->getProperties(pKernelProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hFunction,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLaunchFuncArgs)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendLaunchFunction(hFunction, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchCooperativeKernel(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pLaunchFuncArgs,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hKernel)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLaunchFuncArgs)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendLaunchCooperativeKernel(hKernel, pLaunchFuncArgs, hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchKernelIndirect(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hFunction,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hFunction)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendLaunchFunctionIndirect(hFunction, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendLaunchMultipleKernelsIndirect(
    ze_command_list_handle_t hCommandList,
    uint32_t numFunctions,
    ze_kernel_handle_t *phFunctions,
    const uint32_t *pCountBuffer,
    const ze_group_count_t *pLaunchArgumentsBuffer,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phFunctions)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCountBuffer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pLaunchArgumentsBuffer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendLaunchMultipleFunctionsIndirect(numFunctions, phFunctions, pCountBuffer, pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
