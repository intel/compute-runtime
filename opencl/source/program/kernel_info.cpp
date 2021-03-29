/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"

#include "hw_cmds.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_map>

namespace NEO {

bool useKernelDescriptor = true;

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
    auto &device = dispatchInfo.getClDevice();
    const auto &kernelInfo = dispatchInfo.getKernel()->getKernelInfo();
    this->maxWorkGroupSize = dispatchInfo.getKernel()->getMaxKernelWorkGroupSize();
    this->hasBarriers = kernelInfo.kernelDescriptor.kernelAttributes.usesBarriers();
    this->simdSize = static_cast<uint32_t>(kernelInfo.getMaxSimdSize());
    this->slmTotalSize = static_cast<uint32_t>(dispatchInfo.getKernel()->getSlmTotalSize());
    this->coreFamily = device.getHardwareInfo().platform.eRenderCoreFamily;
    this->numThreadsPerSubSlice = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                  device.getSharedDeviceInfo().numThreadsPerEU;
    this->localMemSize = static_cast<uint32_t>(device.getSharedDeviceInfo().localMemSize);
    setIfUseImg(kernelInfo);
    setMinWorkGroupSize();
}
void WorkSizeInfo::setIfUseImg(const KernelInfo &kernelInfo) {
    for (auto i = 0u; i < kernelInfo.kernelArgInfo.size(); i++) {
        if (kernelInfo.kernelArgInfo[i].isImage) {
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

    delete[] crossThreadData;
}

void KernelInfo::storePatchToken(const SPatchExecutionEnvironment *execEnv) {
    if (execEnv->CompiledForGreaterThan4GBBuffers == false) {
        this->requiresSshForBuffers = true;
    }
    if (execEnv->IndirectStatelessCount > 0) {
        this->hasIndirectStatelessAccess = true;
    }
}

void KernelInfo::storeArgInfo(uint32_t argNum, ArgTypeTraits metadata, std::unique_ptr<ArgTypeMetadataExtended> metadataExtended) {
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
    if (NEO::KernelArgMetadata::AccessUnknown == kernelArgInfo[argNum].metadata.accessQualifier) {
        auto accessQual = pImageMemObjKernelArg->Writeable ? NEO::KernelArgMetadata::AccessReadWrite
                                                           : NEO::KernelArgMetadata::AccessReadOnly;
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
    return kernelDescriptor.payloadMappings.samplerTable.numSamplers;
}
size_t KernelInfo::getSamplerStateArraySize(const HardwareInfo &hwInfo) const {
    size_t samplerStateArraySize = getSamplerStateArrayCount() * Sampler::getSamplerStateSize(hwInfo);
    return samplerStateArraySize;
}

size_t KernelInfo::getBorderColorStateSize() const {
    size_t borderColorSize = 0;
    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0U) {
        borderColorSize = kernelDescriptor.payloadMappings.samplerTable.tableOffset - kernelDescriptor.payloadMappings.samplerTable.borderColor;
    }
    return borderColorSize;
}

size_t KernelInfo::getBorderColorOffset() const {
    size_t borderColorOffset = 0;
    if (kernelDescriptor.payloadMappings.samplerTable.numSamplers > 0U) {
        borderColorOffset = kernelDescriptor.payloadMappings.samplerTable.borderColor;
    }
    return borderColorOffset;
}

uint32_t KernelInfo::getConstantBufferSize() const {
    return kernelDescriptor.kernelAttributes.crossThreadDataSize;
}

bool KernelInfo::createKernelAllocation(const Device &device, bool internalIsa) {
    UNRECOVERABLE_IF(kernelAllocation);
    auto kernelIsaSize = heapInfo.KernelHeapSize;
    const auto allocType = internalIsa ? GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL : GraphicsAllocation::AllocationType::KERNEL_ISA;
    kernelAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties({device.getRootDeviceIndex(), kernelIsaSize, allocType, device.getDeviceBitfield()});
    if (!kernelAllocation) {
        return false;
    }

    auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    return MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *kernelAllocation),
                                                            device, kernelAllocation, 0, heapInfo.pKernelHeap,
                                                            static_cast<size_t>(kernelIsaSize));
}

void KernelInfo::apply(const DeviceInfoKernelPayloadConstants &constants) {
    if (nullptr == this->crossThreadData) {
        return;
    }

    const auto &implicitArgs = kernelDescriptor.payloadMappings.implicitArgs;
    const auto privateMemorySize = static_cast<uint32_t>(KernelHelper::getPrivateSurfaceSize(kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize,
                                                                                             constants.computeUnitsUsedForScratch));

    auto setIfValidOffset = [&](auto value, NEO::CrossThreadDataOffset offset) {
        if (isValidOffset(offset)) {
            *ptrOffset(reinterpret_cast<decltype(value) *>(crossThreadData), offset) = value;
        }
    };
    setIfValidOffset(reinterpret_cast<uintptr_t>(constants.slmWindow), implicitArgs.localMemoryStatelessWindowStartAddres);
    setIfValidOffset(constants.slmWindowSize, implicitArgs.localMemoryStatelessWindowSize);
    setIfValidOffset(privateMemorySize, implicitArgs.privateMemorySize);
    setIfValidOffset(constants.maxWorkGroupSize, implicitArgs.maxWorkGroupSize);
}

std::string concatenateKernelNames(ArrayRef<KernelInfo *> kernelInfos) {
    std::string semiColonDelimitedKernelNameStr;

    for (const auto &kernelInfo : kernelInfos) {
        if (!semiColonDelimitedKernelNameStr.empty()) {
            semiColonDelimitedKernelNameStr += ';';
        }
        semiColonDelimitedKernelNameStr += kernelInfo->kernelDescriptor.kernelMetadata.kernelName;
    }

    return semiColonDelimitedKernelNameStr;
}

} // namespace NEO
