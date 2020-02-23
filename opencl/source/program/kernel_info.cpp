/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_map>

namespace NEO {

struct KernelArgumentType {
    const char *argTypeQualifier;
    uint64_t argTypeQualifierValue;
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
    this->maxWorkGroupSize = dispatchInfo.getKernel()->maxKernelWorkGroupSize;
    auto pExecutionEnvironment = dispatchInfo.getKernel()->getKernelInfo().patchInfo.executionEnvironment;
    this->hasBarriers = (pExecutionEnvironment != nullptr) && (pExecutionEnvironment->HasBarriers);
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

void KernelInfo::storePatchToken(const SPatchExecutionEnvironment *execEnv) {
    this->patchInfo.executionEnvironment = execEnv;
    if (execEnv->RequiredWorkGroupSizeX != 0) {
        this->reqdWorkGroupSize[0] = execEnv->RequiredWorkGroupSizeX;
        this->reqdWorkGroupSize[1] = execEnv->RequiredWorkGroupSizeY;
        this->reqdWorkGroupSize[2] = execEnv->RequiredWorkGroupSizeZ;
        DEBUG_BREAK_IF(!(execEnv->RequiredWorkGroupSizeY > 0));
        DEBUG_BREAK_IF(!(execEnv->RequiredWorkGroupSizeZ > 0));
    }
    this->workgroupWalkOrder[0] = 0;
    this->workgroupWalkOrder[1] = 1;
    this->workgroupWalkOrder[2] = 2;
    if (execEnv->WorkgroupWalkOrderDims) {
        constexpr auto dimensionMask = 0b11;
        constexpr auto dimensionSize = 2;
        this->workgroupWalkOrder[0] = execEnv->WorkgroupWalkOrderDims & dimensionMask;
        this->workgroupWalkOrder[1] = (execEnv->WorkgroupWalkOrderDims >> dimensionSize) & dimensionMask;
        this->workgroupWalkOrder[2] = (execEnv->WorkgroupWalkOrderDims >> dimensionSize * 2) & dimensionMask;
        this->requiresWorkGroupOrder = true;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        // inverts the walk order mapping (from ORDER_ID->DIM_ID to DIM_ID->ORDER_ID)
        this->workgroupDimensionsOrder[this->workgroupWalkOrder[i]] = i;
    }

    if (execEnv->CompiledForGreaterThan4GBBuffers == false) {
        this->requiresSshForBuffers = true;
    }
}

void KernelInfo::storeArgInfo(uint32_t argNum, ArgTypeMetadata metadata, std::unique_ptr<ArgTypeMetadataExtended> metadataExtended) {
    resizeKernelArgInfoAndRegisterParameter(argNum);
    auto &argInfo = kernelArgInfo[argNum];
    argInfo.metadata = metadata;
    argInfo.metadataExtended = std::move(metadataExtended);
    argInfo.isReadOnly |= argInfo.metadata.typeQualifiers.constQual;
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

    kernelArgInfo[argNum].metadata.argByValSize = sizeof(cl_mem);

    kernelArgInfo[argNum].isTransformable = pImageMemObjKernelArg->Transformable != 0;
    patchInfo.imageMemObjKernelArgs.push_back(pImageMemObjKernelArg);
    if (NEO::KernelArgMetadata::AccessQualifier::Unknown == kernelArgInfo[argNum].metadata.accessQualifier) {
        auto accessQual = pImageMemObjKernelArg->Writeable ? NEO::KernelArgMetadata::AccessQualifier::ReadWrite
                                                           : NEO::KernelArgMetadata::AccessQualifier::ReadOnly;
        kernelArgInfo[argNum].metadata.accessQualifier = accessQual;
    }
}

void KernelInfo::storeKernelArgument(
    const SPatchGlobalMemoryObjectKernelArgument *pGlobalMemObjKernelArg) {
    uint32_t argNum = pGlobalMemObjKernelArg->ArgumentNumber;
    uint32_t offsetSurfaceState = pGlobalMemObjKernelArg->Offset;

    usesSsh |= true;
    storeKernelArgPatchInfo(argNum, 0, 0, 0, offsetSurfaceState);
    kernelArgInfo[argNum].isBuffer = true;
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
    kernelArgInfo[argNum].isReadOnly = true;
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
    this->patchInfo.pKernelAttributesInfo = pKernelAttributesInfo;
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

void KernelInfo::storePatchToken(const SPatchAllocateSyncBuffer *pAllocateSyncBuffer) {
    usesSsh |= true;
    patchInfo.pAllocateSyncBuffer = pAllocateSyncBuffer;
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

bool KernelInfo::createKernelAllocation(uint32_t rootDeviceIndex, MemoryManager *memoryManager) {
    UNRECOVERABLE_IF(kernelAllocation);
    auto kernelIsaSize = heapInfo.pKernelHeader->KernelHeapSize;
    kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, kernelIsaSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    if (!kernelAllocation) {
        return false;
    }
    return memoryManager->copyMemoryToAllocation(kernelAllocation, heapInfo.pKernelHeap, kernelIsaSize);
}

void KernelInfo::apply(const DeviceInfoKernelPayloadConstants &constants) {
    if (nullptr == this->crossThreadData) {
        return;
    }

    uint32_t privateMemoryStatelessSizeOffset = this->workloadInfo.privateMemoryStatelessSizeOffset;
    uint32_t localMemoryStatelessWindowSizeOffset = this->workloadInfo.localMemoryStatelessWindowSizeOffset;
    uint32_t localMemoryStatelessWindowStartAddressOffset = this->workloadInfo.localMemoryStatelessWindowStartAddressOffset;

    if (localMemoryStatelessWindowStartAddressOffset != WorkloadInfo::undefinedOffset) {
        *(uintptr_t *)&(this->crossThreadData[localMemoryStatelessWindowStartAddressOffset]) = reinterpret_cast<uintptr_t>(constants.slmWindow);
    }

    if (localMemoryStatelessWindowSizeOffset != WorkloadInfo::undefinedOffset) {
        *(uint32_t *)&(this->crossThreadData[localMemoryStatelessWindowSizeOffset]) = constants.slmWindowSize;
    }

    uint32_t privateMemorySize = 0U;
    if (this->patchInfo.pAllocateStatelessPrivateSurface) {
        privateMemorySize = this->patchInfo.pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize * constants.computeUnitsUsedForScratch * this->getMaxSimdSize();
    }

    if (privateMemoryStatelessSizeOffset != WorkloadInfo::undefinedOffset) {
        *(uint32_t *)&(this->crossThreadData[privateMemoryStatelessSizeOffset]) = privateMemorySize;
    }

    if (this->workloadInfo.maxWorkGroupSizeOffset != WorkloadInfo::undefinedOffset) {
        *(uint32_t *)&(this->crossThreadData[this->workloadInfo.maxWorkGroupSizeOffset]) = constants.maxWorkGroupSize;
    }
}

std::string concatenateKernelNames(ArrayRef<KernelInfo *> kernelInfos) {
    std::string semiColonDelimitedKernelNameStr;

    for (const auto &kernelInfo : kernelInfos) {
        if (!semiColonDelimitedKernelNameStr.empty()) {
            semiColonDelimitedKernelNameStr += ';';
        }
        semiColonDelimitedKernelNameStr += kernelInfo->name;
    }

    return semiColonDelimitedKernelNameStr;
}

} // namespace NEO
