/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/extension_function_address.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/driver_experimental/zex_cmdlist.h"
#include "level_zero/driver_experimental/zex_context.h"
#include "level_zero/ze_intel_gpu.h"
#include "level_zero/zet_intel_gpu_metric.h"

#include <cstring>

namespace L0 {
void *ExtensionFunctionAddressHelper::getExtensionFunctionAddress(const std::string &functionName) {
#define RETURN_FUNC_PTR_IF_EXIST(name)    \
    {                                     \
        if (functionName == #name) {      \
            void *ret = ((void *)(name)); \
            return ret;                   \
        }                                 \
    }

    RETURN_FUNC_PTR_IF_EXIST(zexDriverImportExternalPointer);
    RETURN_FUNC_PTR_IF_EXIST(zexDriverReleaseImportedPointer);
    RETURN_FUNC_PTR_IF_EXIST(zexDriverGetHostPointerBaseAddress);

    RETURN_FUNC_PTR_IF_EXIST(zexKernelGetBaseAddress);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelKernelGetBinaryExp);

    RETURN_FUNC_PTR_IF_EXIST(zexMemGetIpcHandles);
    RETURN_FUNC_PTR_IF_EXIST(zexMemOpenIpcHandles);

    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWaitOnMemory);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWaitOnMemory64);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWriteToMemory);

    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventCreate);
    RETURN_FUNC_PTR_IF_EXIST(zexEventGetDeviceAddress);

    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventCreate2);
    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventGetIpcHandle);
    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventOpenIpcHandle);
    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventCloseIpcHandle);

    RETURN_FUNC_PTR_IF_EXIST(zeMemGetPitchFor2dImage);
    RETURN_FUNC_PTR_IF_EXIST(zeImageGetDeviceOffsetExp);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelGetDriverVersionString);

    RETURN_FUNC_PTR_IF_EXIST(zeIntelMediaCommunicationCreate);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelMediaCommunicationDestroy);

    RETURN_FUNC_PTR_IF_EXIST(zexIntelAllocateNetworkInterrupt);
    RETURN_FUNC_PTR_IF_EXIST(zexIntelReleaseNetworkInterrupt);

    RETURN_FUNC_PTR_IF_EXIST(zeIntelDeviceImportExternalSemaphoreExp);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelCommandListAppendWaitExternalSemaphoresExp);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelCommandListAppendSignalExternalSemaphoresExp);
    RETURN_FUNC_PTR_IF_EXIST(zeIntelDeviceReleaseExternalSemaphoreExp);

#undef RETURN_FUNC_PTR_IF_EXIST

    return ExtensionFunctionAddressHelper::getAdditionalExtensionFunctionAddress(functionName);
}

} // namespace L0
