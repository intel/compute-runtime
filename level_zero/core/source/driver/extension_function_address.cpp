/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/extension_function_address.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"

#include <cstring>

namespace L0 {
void *ExtensionFunctionAddressHelper::getExtensionFunctionAddress(std::string functionName) {
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

    RETURN_FUNC_PTR_IF_EXIST(zexMemGetIpcHandles);
    RETURN_FUNC_PTR_IF_EXIST(zexMemOpenIpcHandles);

    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWaitOnMemory);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWaitOnMemory64);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendWriteToMemory);

    RETURN_FUNC_PTR_IF_EXIST(zexCounterBasedEventCreate);
    RETURN_FUNC_PTR_IF_EXIST(zexEventGetDeviceAddress);
#undef RETURN_FUNC_PTR_IF_EXIST

    return ExtensionFunctionAddressHelper::getAdditionalExtensionFunctionAddress(functionName);
}

} // namespace L0
