/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/sampler/sampler.h"

#include "hw_cmds.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_map>

namespace NEO {

const uint32_t WorkloadInfo::undefinedOffset = (uint32_t)-1;
const uint32_t WorkloadInfo::invalidParentEvent = (uint32_t)-1;

std::unordered_map<std::string, uint32_t> accessQualifierMap = {
    {"", CL_KERNEL_ARG_ACCESS_NONE},
    {"NONE", CL_KERNEL_ARG_ACCESS_NONE},
    {"read_only", CL_KERNEL_ARG_ACCESS_READ_ONLY},
    {"__read_only", CL_KERNEL_ARG_ACCESS_READ_ONLY},
    {"write_only", CL_KERNEL_ARG_ACCESS_WRITE_ONLY},
    {"__write_only", CL_KERNEL_ARG_ACCESS_WRITE_ONLY},
    {"read_write", CL_KERNEL_ARG_ACCESS_READ_WRITE},
    {"__read_write", CL_KERNEL_ARG_ACCESS_READ_WRITE},
};

std::unordered_map<std::string, uint32_t> addressQualifierMap = {
    {"", CL_KERNEL_ARG_ADDRESS_GLOBAL},
    {"__global", CL_KERNEL_ARG_ADDRESS_GLOBAL},
    {"__local", CL_KERNEL_ARG_ADDRESS_LOCAL},
    {"__private", CL_KERNEL_ARG_ADDRESS_PRIVATE},
    {"__constant", CL_KERNEL_ARG_ADDRESS_CONSTANT},
    {"not_specified", CL_KERNEL_ARG_ADDRESS_PRIVATE},
};

struct KernelArgumentType {
    const char *argTypeQualifier;
    uint64_t argTypeQualifierValue;
};

constexpr KernelArgumentType typeQualifiers[] = {
    {"const", CL_KERNEL_ARG_TYPE_CONST},
    {"volatile", CL_KERNEL_ARG_TYPE_VOLATILE},
    {"restrict", CL_KERNEL_ARG_TYPE_RESTRICT},
    {"pipe", CL_KERNEL_ARG_TYPE_PIPE},
};

std::map<std::string, size_t> typeSizeMap = {
    {"char", sizeof(cl_char)},
    {"char2", sizeof(cl_char2)},
    {"char3", sizeof(cl_char3)},
    {"char4", sizeof(cl_char4)},
    {"char8", sizeof(cl_char8)},
    {"char16", sizeof(cl_char16)},

    {"uchar", sizeof(cl_uchar)},
    {"uchar2", sizeof(cl_uchar2)},
    {"uchar3", sizeof(cl_uchar3)},
    {"uchar4", sizeof(cl_uchar4)},
    {"uchar8", sizeof(cl_uchar8)},
    {"uchar16", sizeof(cl_uchar16)},

    {"short", sizeof(cl_short)},
    {"short2", sizeof(cl_short2)},
    {"short3", sizeof(cl_short3)},
    {"short4", sizeof(cl_short4)},
    {"short8", sizeof(cl_short8)},
    {"short16", sizeof(cl_short16)},

    {"ushort", sizeof(cl_ushort)},
    {"ushort2", sizeof(cl_ushort2)},
    {"ushort3", sizeof(cl_ushort3)},
    {"ushort4", sizeof(cl_ushort4)},
    {"ushort8", sizeof(cl_ushort8)},
    {"ushort16", sizeof(cl_ushort16)},

    {"int", sizeof(cl_int)},
    {"int2", sizeof(cl_int2)},
    {"int3", sizeof(cl_int3)},
    {"int4", sizeof(cl_int4)},
    {"int8", sizeof(cl_int8)},
    {"int16", sizeof(cl_int16)},

    {"uint", sizeof(cl_uint)},
    {"uint2", sizeof(cl_uint2)},
    {"uint3", sizeof(cl_uint3)},
    {"uint4", sizeof(cl_uint4)},
    {"uint8", sizeof(cl_uint8)},
    {"uint16", sizeof(cl_uint16)},

    {"long", sizeof(cl_long)},
    {"long2", sizeof(cl_long2)},
    {"long3", sizeof(cl_long3)},
    {"long4", sizeof(cl_long4)},
    {"long8", sizeof(cl_long8)},
    {"long16", sizeof(cl_long16)},

    {"ulong", sizeof(cl_ulong)},
    {"ulong2", sizeof(cl_ulong2)},
    {"ulong3", sizeof(cl_ulong3)},
    {"ulong4", sizeof(cl_ulong4)},
    {"ulong8", sizeof(cl_ulong8)},
    {"ulong16", sizeof(cl_ulong16)},

    {"half", sizeof(cl_half)},

    {"float", sizeof(cl_float)},
    {"float2", sizeof(cl_float2)},
    {"float3", sizeof(cl_float3)},
    {"float4", sizeof(cl_float4)},
    {"float8", sizeof(cl_float8)},
    {"float16", sizeof(cl_float16)},

#ifdef cl_khr_fp16
    {"half2", sizeof(cl_half2)},
    {"half3", sizeof(cl_half3)},
    {"half4", sizeof(cl_half4)},
    {"half8", sizeof(cl_half8)},
    {"half16", sizeof(cl_half16)},
#endif

    {"double", sizeof(cl_double)},
    {"double2", sizeof(cl_double2)},
    {"double3", sizeof(cl_double3)},
    {"double4", sizeof(cl_double4)},
    {"double8", sizeof(cl_double8)},
    {"double16", sizeof(cl_double16)},
};
WorkSizeInfo::WorkSizeInfo(uint32_t maxWorkGroupSize, bool hasBarriers, uint32_t simdSize, uint32_t slmTotalSize, GFXCORE_FAMILY coreFamily, uint32_t numThreadsPerSubSlice, uint32_t localMemSize, bool imgUsed, bool yTiledSurface) {
    this->maxWorkGroupSize = maxWorkGroupSize;
    this->hasBarriers = hasBarriers;
    this->simdSize = simdSize;
    this->slmTotalSize = slmTotalSize;
    this->coreFamily = coreFamily;
    this->numThreadsPerSubSlice = numThreadsPerSubSlice;
    this->localMemSize = localMemSize;
    this->imgUsed = imgUsed;
    this->yTiledSurfaces = yTiledSurface;
    setMinWorkGroupSize();
}
WorkSizeInfo::WorkSizeInfo(const DispatchInfo &dispatchInfo) {
    this->maxWorkGroupSize = (uint32_t)dispatchInfo.getKernel()->getDevice().getDeviceInfo().maxWorkGroupSize;
    this->hasBarriers = !!dispatchInfo.getKernel()->getKernelInfo().patchInfo.executionEnvironment->HasBarriers;
    this->simdSize = (uint32_t)dispatchInfo.getKernel()->getKernelInfo().getMaxSimdSize();
    this->slmTotalSize = (uint32_t)dispatchInfo.getKernel()->slmTotalSize;
    this->coreFamily = dispatchInfo.getKernel()->getDevice().getHardwareInfo().platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = (uint32_t)dispatchInfo.getKernel()->getDevice().getDeviceInfo().maxNumEUsPerSubSlice * dispatchInfo.getKernel()->getDevice().getDeviceInfo().numThreadsPerEU;
    this->localMemSize = (uint32_t)dispatchInfo.getKernel()->getDevice().getDeviceInfo().localMemSize;
    setIfUseImg(dispatchInfo.getKernel());
    setMinWorkGroupSize();
}
void WorkSizeInfo::setIfUseImg(Kernel *pKernel) {
    auto ParamsCount = pKernel->getKernelArgsNumber();
    for (auto i = 0u; i < ParamsCount; i++) {
        if (pKernel->getKernelInfo().kernelArgInfo[i].isImage) {
            imgUsed = true;
            yTiledSurfaces = true;
        }
    }
}
void WorkSizeInfo::setMinWorkGroupSize() {
    minWorkGroupSize = 0;
    if (hasBarriers) {
        uint32_t maxBarriersPerHSlice = (coreFamily >= IGFX_GEN9_CORE) ? 32 : 16;
        minWorkGroupSize = numThreadsPerSubSlice * simdSize / maxBarriersPerHSlice;
    }
    if (slmTotalSize > 0) {
        minWorkGroupSize = std::max(maxWorkGroupSize / ((localMemSize / slmTotalSize)), minWorkGroupSize);
    }
}
void WorkSizeInfo::checkRatio(const size_t workItems[3]) {
    if (slmTotalSize > 0) {
        useRatio = true;
        targetRatio = log((float)workItems[0]) - log((float)workItems[1]);
        useStrictRatio = false;
    } else if (yTiledSurfaces == true) {
        useRatio = true;
        targetRatio = YTilingRatioValue;
        useStrictRatio = true;
    }
}

KernelInfo::~KernelInfo() {
    kernelArgInfo.clear();

    patchInfo.stringDataMap.clear();
    delete[] crossThreadData;
}

cl_int KernelInfo::storeArgInfo(const SPatchKernelArgumentInfo *pkernelArgInfo) {
    cl_int retVal = CL_SUCCESS;

    if (pkernelArgInfo == nullptr) {
        retVal = CL_INVALID_BINARY;
    } else {
        uint32_t argNum = pkernelArgInfo->ArgumentNumber;
        auto pCurArgAttrib = ptrOffset(
            reinterpret_cast<const char *>(pkernelArgInfo),
            sizeof(SPatchKernelArgumentInfo));

        resizeKernelArgInfoAndRegisterParameter(argNum);

        kernelArgInfo[argNum].addressQualifierStr = pCurArgAttrib;
        pCurArgAttrib += pkernelArgInfo->AddressQualifierSize;

        kernelArgInfo[argNum].accessQualifierStr = pCurArgAttrib;
        pCurArgAttrib += pkernelArgInfo->AccessQualifierSize;

        kernelArgInfo[argNum].name = pCurArgAttrib;
        pCurArgAttrib += pkernelArgInfo->ArgumentNameSize;

        {
            auto argType = strchr(pCurArgAttrib, ';');
            DEBUG_BREAK_IF(argType == nullptr);

            kernelArgInfo[argNum].typeStr.assign(pCurArgAttrib, argType - pCurArgAttrib);
            pCurArgAttrib += pkernelArgInfo->TypeNameSize;

            ++argType;
        }

        kernelArgInfo[argNum].typeQualifierStr = pCurArgAttrib;

        patchInfo.kernelArgumentInfo.push_back(pkernelArgInfo);
    }

    return retVal;
}

void KernelInfo::storeKernelArgument(
    const SPatchDataParameterBuffer *pDataParameterKernelArg) {
    uint32_t argNum = pDataParameterKernelArg->ArgumentNumber;
    uint32_t dataSize = pDataParameterKernelArg->DataSize;
    uint32_t offset = pDataParameterKernelArg->Offset;
    uint32_t sourceOffset = pDataParameterKernelArg->SourceOffset;

    storeKernelArgPatchInfo(argNum, dataSize, offset, sourceOffset, 0);
}

void KernelInfo::storeKernelArgument(
    const SPatchStatelessGlobalMemoryObjectKernelArgument *pStatelessGlobalKernelArg) {
    uint32_t argNum = pStatelessGlobalKernelArg->ArgumentNumber;
    uint32_t offsetSSH = pStatelessGlobalKernelArg->SurfaceStateHeapOffset;

    usesSsh |= true;
    storeKernelArgPatchInfo(argNum, pStatelessGlobalKernelArg->DataParamSize, pStatelessGlobalKernelArg->DataParamOffset, 0, offsetSSH);
    kernelArgInfo[argNum].isBuffer = true;
    patchInfo.statelessGlobalMemObjKernelArgs.push_back(pStatelessGlobalKernelArg);
}

void KernelInfo::storeKernelArgument(
    const SPatchImageMemoryObjectKernelArgument *pImageMemObjKernelArg) {
    uint32_t argNum = pImageMemObjKernelArg->ArgumentNumber;
    uint32_t offsetSurfaceState = pImageMemObjKernelArg->Offset;

    usesSsh |= true;
    storeKernelArgPatchInfo(argNum, 0, 0, 0, offsetSurfaceState);
    kernelArgInfo[argNum].isImage = true;

    if (pImageMemObjKernelArg->Type == iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA) {
        kernelArgInfo[argNum].isMediaImage = true;
    }

    if (pImageMemObjKernelArg->Type == iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK) {
        kernelArgInfo[argNum].isMediaBlockImage = true;
    }

    kernelArgInfo[argNum].accessQualifier = pImageMemObjKernelArg->Writeable
                                                ? CL_KERNEL_ARG_ACCESS_READ_WRITE
                                                : CL_KERNEL_ARG_ACCESS_READ_ONLY;

    kernelArgInfo[argNum].isTransformable = pImageMemObjKernelArg->Transformable != 0;
    patchInfo.imageMemObjKernelArgs.push_back(pImageMemObjKernelArg);
}

void KernelInfo::storeKernelArgument(
    const SPatchGlobalMemoryObjectKernelArgument *pGlobalMemObjKernelArg) {
    uint32_t argNum = pGlobalMemObjKernelArg->ArgumentNumber;
    uint32_t offsetSurfaceState = pGlobalMemObjKernelArg->Offset;

    usesSsh |= true;
    storeKernelArgPatchInfo(argNum, 0, 0, 0, offsetSurfaceState);
    kernelArgInfo[argNum].isBuffer = true;

    patchInfo.globalMemObjKernelArgs.push_back(pGlobalMemObjKernelArg);
}

void KernelInfo::storeKernelArgument(
    const SPatchSamplerKernelArgument *pSamplerArgument) {
    uint32_t argNum = pSamplerArgument->ArgumentNumber;
    uint32_t offsetSurfaceState = pSamplerArgument->Offset;

    storeKernelArgPatchInfo(argNum, 0, 0, 0, offsetSurfaceState);
    kernelArgInfo[argNum].samplerArgumentType = pSamplerArgument->Type;

    if (pSamplerArgument->Type != iOpenCL::SAMPLER_OBJECT_TEXTURE) {
        DEBUG_BREAK_IF(pSamplerArgument->Type != iOpenCL::SAMPLER_OBJECT_VME &&
                       pSamplerArgument->Type != iOpenCL::SAMPLER_OBJECT_VE &&
                       pSamplerArgument->Type != iOpenCL::SAMPLER_OBJECT_VD);
        kernelArgInfo[argNum].isAccelerator = true;
        isVmeWorkload = true;
    } else {
        kernelArgInfo[argNum].isSampler = true;
    }
}

void KernelInfo::storeKernelArgument(
    const SPatchStatelessConstantMemoryObjectKernelArgument *pStatelessConstMemObjKernelArg) {
    uint32_t argNum = pStatelessConstMemObjKernelArg->ArgumentNumber;
    uint32_t offsetSSH = pStatelessConstMemObjKernelArg->SurfaceStateHeapOffset;

    usesSsh |= true;
    storeKernelArgPatchInfo(argNum, pStatelessConstMemObjKernelArg->DataParamSize, pStatelessConstMemObjKernelArg->DataParamOffset, 0, offsetSSH);
    kernelArgInfo[argNum].isBuffer = true;
    patchInfo.statelessGlobalMemObjKernelArgs.push_back(reinterpret_cast<const SPatchStatelessGlobalMemoryObjectKernelArgument *>(pStatelessConstMemObjKernelArg));
}

void KernelInfo::storeKernelArgument(const SPatchStatelessDeviceQueueKernelArgument *pStatelessDeviceQueueKernelArg) {
    uint32_t argNum = pStatelessDeviceQueueKernelArg->ArgumentNumber;

    resizeKernelArgInfoAndRegisterParameter(argNum);
    kernelArgInfo[argNum].isDeviceQueue = true;

    storeKernelArgPatchInfo(argNum, pStatelessDeviceQueueKernelArg->DataParamSize, pStatelessDeviceQueueKernelArg->DataParamOffset, 0, pStatelessDeviceQueueKernelArg->SurfaceStateHeapOffset);
}

void KernelInfo::storePatchToken(
    const SPatchAllocateStatelessPrivateSurface *pStatelessPrivateSurfaceArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessPrivateSurface = pStatelessPrivateSurfaceArg;
}

void KernelInfo::storePatchToken(const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *pStatelessConstantMemorySurfaceWithInitializationArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = pStatelessConstantMemorySurfaceWithInitializationArg;
}

void KernelInfo::storePatchToken(const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *pStatelessGlobalMemorySurfaceWithInitializationArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = pStatelessGlobalMemorySurfaceWithInitializationArg;
}

void KernelInfo::storePatchToken(const SPatchAllocateStatelessPrintfSurface *pStatelessPrintfSurfaceArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessPrintfSurface = pStatelessPrintfSurfaceArg;
}

void KernelInfo::storePatchToken(const SPatchAllocateStatelessEventPoolSurface *pStatelessEventPoolSurfaceArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessEventPoolSurface = pStatelessEventPoolSurfaceArg;
}

void KernelInfo::storePatchToken(const SPatchAllocateStatelessDefaultDeviceQueueSurface *pStatelessDefaultDeviceQueueSurfaceArg) {
    usesSsh |= true;
    patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = pStatelessDefaultDeviceQueueSurfaceArg;
}

void KernelInfo::storePatchToken(const SPatchString *pStringArg) {
    uint32_t stringIndex = pStringArg->Index;
    if (pStringArg->StringSize > 0) {
        const char *stringData = reinterpret_cast<const char *>(pStringArg + 1);
        patchInfo.stringDataMap.emplace(stringIndex, std::string(stringData, stringData + pStringArg->StringSize));
    }
}

void KernelInfo::storePatchToken(const SPatchKernelAttributesInfo *pKernelAttributesInfo) {
    attributes = reinterpret_cast<const char *>(pKernelAttributesInfo) + sizeof(SPatchKernelAttributesInfo);

    auto start = attributes.find("intel_reqd_sub_group_size(");
    if (start != std::string::npos) {
        start += strlen("intel_reqd_sub_group_size(");
        auto stop = attributes.find(")", start);
        std::stringstream requiredSubGroupSizeStr(attributes.substr(start, stop - start));
        requiredSubGroupSizeStr >> requiredSubGroupSize;
    }
}

void KernelInfo::storePatchToken(const SPatchAllocateSystemThreadSurface *pSystemThreadSurface) {
    usesSsh |= true;
    patchInfo.pAllocateSystemThreadSurface = pSystemThreadSurface;
}

cl_int KernelInfo::resolveKernelInfo() {
    cl_int retVal = CL_SUCCESS;
    std::unordered_map<std::string, uint32_t>::iterator iterUint;
    std::unordered_map<std::string, size_t>::iterator iterSizeT;

    for (auto &argInfo : kernelArgInfo) {
        iterUint = accessQualifierMap.find(argInfo.accessQualifierStr);
        if (iterUint != accessQualifierMap.end()) {
            argInfo.accessQualifier = iterUint->second;
        } else {
            retVal = CL_INVALID_BINARY;
            break;
        }

        iterUint = addressQualifierMap.find(argInfo.addressQualifierStr);
        if (iterUint != addressQualifierMap.end()) {
            argInfo.addressQualifier = iterUint->second;
        } else {
            retVal = CL_INVALID_BINARY;
            break;
        }

        auto qualifierCount = sizeof(typeQualifiers) / sizeof(typeQualifiers[0]);

        for (auto qualifierId = 0u; qualifierId < qualifierCount; qualifierId++) {
            if (strstr(argInfo.typeQualifierStr.c_str(), typeQualifiers[qualifierId].argTypeQualifier) != nullptr) {
                argInfo.typeQualifier |= typeQualifiers[qualifierId].argTypeQualifierValue;
            }
        }
    }

    return retVal;
}

void KernelInfo::storeKernelArgPatchInfo(uint32_t argNum, uint32_t dataSize, uint32_t dataOffset, uint32_t sourceOffset, uint32_t offsetSSH) {
    resizeKernelArgInfoAndRegisterParameter(argNum);

    KernelArgPatchInfo kernelArgPatchInfo;
    kernelArgPatchInfo.crossthreadOffset = dataOffset;
    kernelArgPatchInfo.size = dataSize;
    kernelArgPatchInfo.sourceOffset = sourceOffset;

    kernelArgInfo[argNum].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelArgInfo[argNum].offsetHeap = offsetSSH;
}

size_t KernelInfo::getSamplerStateArrayCount() const {
    size_t count = patchInfo.samplerStateArray ? (size_t)patchInfo.samplerStateArray->Count : 0;
    return count;
}
size_t KernelInfo::getSamplerStateArraySize(const HardwareInfo &hwInfo) const {
    size_t samplerStateArraySize = getSamplerStateArrayCount() * Sampler::getSamplerStateSize(hwInfo);
    return samplerStateArraySize;
}

size_t KernelInfo::getBorderColorStateSize() const {
    size_t borderColorSize = 0;
    if (patchInfo.samplerStateArray) {
        borderColorSize = patchInfo.samplerStateArray->Offset - patchInfo.samplerStateArray->BorderColorOffset;
    }
    return borderColorSize;
}

size_t KernelInfo::getBorderColorOffset() const {
    size_t borderColorOffset = 0;
    if (patchInfo.samplerStateArray) {
        borderColorOffset = patchInfo.samplerStateArray->BorderColorOffset;
    }
    return borderColorOffset;
}

uint32_t KernelInfo::getConstantBufferSize() const {
    return patchInfo.dataParameterStream ? patchInfo.dataParameterStream->DataParameterStreamSize : 0;
}

bool KernelInfo::createKernelAllocation(MemoryManager *memoryManager) {
    UNRECOVERABLE_IF(kernelAllocation);
    auto kernelIsaSize = heapInfo.pKernelHeader->KernelHeapSize;
    kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties({kernelIsaSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    if (!kernelAllocation) {
        return false;
    }
    return memoryManager->copyMemoryToAllocation(kernelAllocation, heapInfo.pKernelHeap, kernelIsaSize);
}

} // namespace NEO
