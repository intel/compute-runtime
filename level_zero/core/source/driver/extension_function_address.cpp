/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/extension_function_address.h"

#include "level_zero/api/extensions/public/ze_exp_ext.h"
#include "level_zero/driver_experimental/mcl_ext/zex_mutable_cmdlist_ext.h"
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
    RETURN_FUNC_PTR_IF_EXIST(zeDriverGetDefaultContext);
    RETURN_FUNC_PTR_IF_EXIST(zerDriverGetDefaultContext);

    RETURN_FUNC_PTR_IF_EXIST(zerDeviceTranslateToIdentifier);
    RETURN_FUNC_PTR_IF_EXIST(zerIdentifierTranslateToDeviceHandle);
    RETURN_FUNC_PTR_IF_EXIST(zeDeviceSynchronize);
    RETURN_FUNC_PTR_IF_EXIST(zeDeviceGetPriorityLevels);

    RETURN_FUNC_PTR_IF_EXIST(zeCommandListAppendLaunchKernelWithArguments);

    RETURN_FUNC_PTR_IF_EXIST(zexKernelGetBaseAddress);
    RETURN_FUNC_PTR_IF_EXIST(zexKernelGetArgumentSize);
    RETURN_FUNC_PTR_IF_EXIST(zexKernelGetArgumentType);

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

    RETURN_FUNC_PTR_IF_EXIST(zetIntelCommandListAppendMarkerExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelDeviceEnableMetricsExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelDeviceDisableMetricsExp);

    // mutable command list extension
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListGetNextCommandIdExp);
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListUpdateMutableCommandsExp);
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListUpdateMutableCommandSignalEventExp);
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListUpdateMutableCommandWaitEventsExp);
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListGetNextCommandIdWithKernelsExp);
    RETURN_FUNC_PTR_IF_EXIST(zeCommandListUpdateMutableCommandKernelsExp);

    RETURN_FUNC_PTR_IF_EXIST(zexCommandListGetVariable);
    RETURN_FUNC_PTR_IF_EXIST(zexKernelSetArgumentVariable);
    RETURN_FUNC_PTR_IF_EXIST(zexVariableSetValue);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListGetLabel);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListSetLabel);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendJump);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendLoadRegVariable);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendStoreRegVariable);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListTempMemSetEleCount);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListTempMemGetSize);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListTempMemSet);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListGetNativeBinary);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListLoadNativeBinary);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListAppendVariableLaunchKernel);
    RETURN_FUNC_PTR_IF_EXIST(zexKernelSetVariableGroupSize);
    RETURN_FUNC_PTR_IF_EXIST(zexVariableGetInfo);
    RETURN_FUNC_PTR_IF_EXIST(zexCommandListGetVariablesList);

    // Metrics
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerCreateExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerDestroyExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerEnableExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerDisableExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerReadDataExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricDecoderCreateExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricDecoderDestroyExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricDecoderGetDecodableMetricsExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricTracerDecodeExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricCalculateOperationCreateExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricCalculateOperationDestroyExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricCalculateGetReportFormatExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricDecodeCalculateMultipleValuesExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricCalculateMultipleValuesExp);
    RETURN_FUNC_PTR_IF_EXIST(zetIntelMetricCalculateValuesExp);

#undef RETURN_FUNC_PTR_IF_EXIST

    return ExtensionFunctionAddressHelper::getAdditionalExtensionFunctionAddress(functionName);
}

} // namespace L0
