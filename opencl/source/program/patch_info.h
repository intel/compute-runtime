/*
 * Copyright (C) 2017-2021 Intel Corporation
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
    ::std::vector<const SPatchStatelessGlobalMemoryObjectKernelArgument *>
        statelessGlobalMemObjKernelArgs;
    ::std::vector<const SPatchImageMemoryObjectKernelArgument *>
        imageMemObjKernelArgs;
};

} // namespace NEO
