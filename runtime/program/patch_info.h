/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "patch_list.h"
#include "patch_g7.h"
#include <vector>
#include <map>

namespace OCLRT {
using iOpenCL::SKernelBinaryHeaderCommon;
using iOpenCL::SPatchAllocateLocalSurface;
using iOpenCL::SPatchAllocateStatelessConstantMemorySurfaceWithInitialization;
using iOpenCL::SPatchAllocateStatelessDefaultDeviceQueueSurface;
using iOpenCL::SPatchAllocateStatelessEventPoolSurface;
using iOpenCL::SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization;
using iOpenCL::SPatchAllocateStatelessPrintfSurface;
using iOpenCL::SPatchAllocateStatelessPrivateSurface;
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

typedef struct TagPrintfStringInfo {
    size_t SizeInBytes;
    char *pStringData;
} PrintfStringInfo, *PPrintfStringInfo;

struct PatchInfo {
    const SPatchMediaInterfaceDescriptorLoad *interfaceDescriptorDataLoad = nullptr;
    const SPatchAllocateLocalSurface *localsurface = nullptr;
    const SPatchMediaVFEState *mediavfestate = nullptr;
    const SPatchInterfaceDescriptorData *interfaceDescriptorData = nullptr;
    const SPatchSamplerStateArray *samplerStateArray = nullptr;
    const SPatchBindingTableState *bindingTableState = nullptr;
    ::std::vector<const SPatchDataParameterBuffer *> dataParameterBuffers;
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
    const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pAllocateStatelessConstantMemorySurfaceWithInitialization = nullptr;
    const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pAllocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
    const SPatchAllocateStatelessPrintfSurface *pAllocateStatelessPrintfSurface = nullptr;
    const SPatchAllocateStatelessEventPoolSurface *pAllocateStatelessEventPoolSurface = nullptr;
    const SPatchAllocateStatelessDefaultDeviceQueueSurface *pAllocateStatelessDefaultDeviceQueueSurface = nullptr;
    const SPatchAllocateSystemThreadSurface *pAllocateSystemThreadSurface = nullptr;
    ::std::map<uint32_t, PrintfStringInfo> stringDataMap;
    ::std::vector<const SPatchKernelArgumentInfo *> kernelArgumentInfo;

    PatchInfo() {
    }
};

} // namespace OCLRT
