/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "patch_g7.h"
#include "patch_list.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
using iOpenCL::SKernelBinaryHeaderCommon;
using iOpenCL::SPatchAllocateLocalSurface;
using iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization;
using iOpenCL::SPatchAllocateStatelessDefaultDeviceQueueSurface;
using iOpenCL::SPatchAllocateStatelessEventPoolSurface;
using iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization;
using iOpenCL::SPatchAllocateStatelessPrintfSurface;
using iOpenCL::SPatchAllocateStatelessPrivateSurface;
using iOpenCL::SPatchAllocateSyncBuffer;
using iOpenCL::SPatchAllocateSystemThreadSurface;
using iOpenCL::SPatchBindingTableState;
using iOpenCL::SPatchDataParameterBuffer;
using iOpenCL::SPatchDataParameterStream;
using iOpenCL::SPatchExecutionEnvironment;
using iOpenCL::SPatchGlobalMemoryObjectKernelArgument;
using iOpenCL::SPatchGtpinFreeGRFInfo;
using iOpenCL::SPatchImageMemoryObjectKernelArgument;
using iOpenCL::SPatchInterfaceDescriptorData;
using iOpenCL::SPatchKernelArgumentInfo;
using iOpenCL::SPatchKernelAttributesInfo;
using iOpenCL::SPatchMediaInterfaceDescriptorLoad;
using iOpenCL::SPatchMediaVFEState;
using iOpenCL::SPatchSamplerKernelArgument;
using iOpenCL::SPatchSamplerStateArray;
using iOpenCL::SPatchStatelessConstantMemoryObjectKernelArgument;
using iOpenCL::SPatchStatelessDeviceQueueKernelArgument;
using iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument;
using iOpenCL::SPatchStateSIP;
using iOpenCL::SPatchString;
using iOpenCL::SPatchThreadPayload;
using iOpenCL::SProgramBinaryHeader;

struct PatchInfo {
    const SPatchMediaInterfaceDescriptorLoad *interfaceDescriptorDataLoad = nullptr;
    const SPatchAllocateLocalSurface *localsurface = nullptr;
    const SPatchMediaVFEState *mediavfestate = nullptr;
    const SPatchMediaVFEState *mediaVfeStateSlot1 = nullptr;
    const SPatchInterfaceDescriptorData *interfaceDescriptorData = nullptr;
    const SPatchSamplerStateArray *samplerStateArray = nullptr;
    const SPatchBindingTableState *bindingTableState = nullptr;
    ::std::vector<const SPatchDataParameterBuffer *> dataParameterBuffersKernelArgs;
    ::std::vector<const SPatchStatelessGlobalMemoryObjectKernelArgument *>
        statelessGlobalMemObjKernelArgs;
    ::std::vector<const SPatchImageMemoryObjectKernelArgument *>
        imageMemObjKernelArgs;
    ::std::vector<const SPatchGlobalMemoryObjectKernelArgument *>
        globalMemObjKernelArgs;
    const SPatchDataParameterStream *dataParameterStream = nullptr;
    const SPatchThreadPayload *threadPayload = nullptr;
    const SPatchExecutionEnvironment *executionEnvironment = nullptr;
    const SPatchKernelAttributesInfo *pKernelAttributesInfo = nullptr;
    const SPatchAllocateStatelessPrivateSurface *pAllocateStatelessPrivateSurface = nullptr;
    const SPatchAllocateSyncBuffer *pAllocateSyncBuffer = nullptr;
    const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pAllocateStatelessConstantMemorySurfaceWithInitialization = nullptr;
    const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pAllocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
    const SPatchAllocateStatelessPrintfSurface *pAllocateStatelessPrintfSurface = nullptr;
    const SPatchAllocateStatelessEventPoolSurface *pAllocateStatelessEventPoolSurface = nullptr;
    const SPatchAllocateStatelessDefaultDeviceQueueSurface *pAllocateStatelessDefaultDeviceQueueSurface = nullptr;
    const SPatchAllocateSystemThreadSurface *pAllocateSystemThreadSurface = nullptr;
    ::std::unordered_map<uint32_t, std::string> stringDataMap;
    ::std::vector<const SPatchKernelArgumentInfo *> kernelArgumentInfo;
};

} // namespace NEO
