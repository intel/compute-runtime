/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/program/kernel_info_from_patchtokens.h"

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "opencl/source/program/kernel_info.h"

#include <cstring>

namespace NEO {

using namespace iOpenCL;

template <typename T>
inline void storeTokenIfNotNull(KernelInfo &kernelInfo, T *token) {
    if (token != nullptr) {
        kernelInfo.storePatchToken(token);
    }
}

template <typename T>
inline uint32_t getOffset(T *token) {
    if (token != nullptr) {
        return token->Offset;
    }
    return WorkloadInfo::undefinedOffset;
}

void populateKernelInfoArgMetadata(KernelInfo &dstKernelInfoArg, const SPatchKernelArgumentInfo *src) {
    if (nullptr == src) {
        return;
    }

    uint32_t argNum = src->ArgumentNumber;

    auto inlineData = PatchTokenBinary::getInlineData(src);

    auto metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    metadataExtended->addressQualifier = parseLimitedString(inlineData.addressQualifier.begin(), inlineData.addressQualifier.size());
    metadataExtended->accessQualifier = parseLimitedString(inlineData.accessQualifier.begin(), inlineData.accessQualifier.size());
    metadataExtended->argName = parseLimitedString(inlineData.argName.begin(), inlineData.argName.size());

    auto argTypeFull = parseLimitedString(inlineData.typeName.begin(), inlineData.typeName.size());
    const char *argTypeDelim = strchr(argTypeFull.data(), ';');
    if (nullptr == argTypeDelim) {
        argTypeDelim = argTypeFull.data() + argTypeFull.size();
    }
    metadataExtended->type = std::string(argTypeFull.data(), argTypeDelim).c_str();
    metadataExtended->typeQualifiers = parseLimitedString(inlineData.typeQualifiers.begin(), inlineData.typeQualifiers.size());

    ArgTypeTraits metadata = {};
    metadata.accessQualifier = KernelArgMetadata::parseAccessQualifier(metadataExtended->accessQualifier);
    metadata.addressQualifier = KernelArgMetadata::parseAddressSpace(metadataExtended->addressQualifier);
    metadata.typeQualifiers = KernelArgMetadata::parseTypeQualifiers(metadataExtended->typeQualifiers);

    dstKernelInfoArg.storeArgInfo(argNum, metadata, std::move(metadataExtended));
}

void populateKernelInfoArg(KernelInfo &dstKernelInfo, KernelArgInfo &dstKernelInfoArg, const PatchTokenBinary::KernelArgFromPatchtokens &src) {
    populateKernelInfoArgMetadata(dstKernelInfo, src.argInfo);
    if (src.objectArg != nullptr) {
        switch (src.objectArg->Token) {
        default:
            UNRECOVERABLE_IF(PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT != src.objectArg->Token);
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchImageMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT:
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchSamplerKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchGlobalMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchStatelessGlobalMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT:
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchStatelessConstantMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT:
            dstKernelInfo.storeKernelArgument(reinterpret_cast<const SPatchStatelessDeviceQueueKernelArgument *>(src.objectArg));
            break;
        }
    }

    switch (src.objectType) {
    default:
        UNRECOVERABLE_IF(PatchTokenBinary::ArgObjectType::None != src.objectType);
        break;
    case PatchTokenBinary::ArgObjectType::Buffer:
        dstKernelInfoArg.offsetBufferOffset = getOffset(src.metadata.buffer.bufferOffset);
        dstKernelInfoArg.pureStatefulBufferAccess = (src.metadata.buffer.pureStateful != nullptr);
        break;
    case PatchTokenBinary::ArgObjectType::Image:
        dstKernelInfoArg.offsetImgWidth = getOffset(src.metadata.image.width);
        dstKernelInfoArg.offsetImgHeight = getOffset(src.metadata.image.height);
        dstKernelInfoArg.offsetImgDepth = getOffset(src.metadata.image.depth);
        dstKernelInfoArg.offsetChannelDataType = getOffset(src.metadata.image.channelDataType);
        dstKernelInfoArg.offsetChannelOrder = getOffset(src.metadata.image.channelOrder);
        dstKernelInfoArg.offsetArraySize = getOffset(src.metadata.image.arraySize);
        dstKernelInfoArg.offsetNumSamples = getOffset(src.metadata.image.numSamples);
        dstKernelInfoArg.offsetNumMipLevels = getOffset(src.metadata.image.numMipLevels);
        dstKernelInfoArg.offsetFlatBaseOffset = getOffset(src.metadata.image.flatBaseOffset);
        dstKernelInfoArg.offsetFlatWidth = getOffset(src.metadata.image.flatWidth);
        dstKernelInfoArg.offsetFlatHeight = getOffset(src.metadata.image.flatHeight);
        dstKernelInfoArg.offsetFlatPitch = getOffset(src.metadata.image.flatPitch);
        break;
    case PatchTokenBinary::ArgObjectType::Sampler:
        dstKernelInfoArg.offsetSamplerSnapWa = getOffset(src.metadata.sampler.coordinateSnapWaRequired);
        dstKernelInfoArg.offsetSamplerAddressingMode = getOffset(src.metadata.sampler.addressMode);
        dstKernelInfoArg.offsetSamplerNormalizedCoords = getOffset(src.metadata.sampler.normalizedCoords);
        break;
    case PatchTokenBinary::ArgObjectType::Slm:
        dstKernelInfoArg.slmAlignment = src.metadata.slm.token->SourceOffset;
        break;
    }

    switch (src.objectTypeSpecialized) {
    default:
        UNRECOVERABLE_IF(PatchTokenBinary::ArgObjectTypeSpecialized::None != src.objectTypeSpecialized);
        break;
    case PatchTokenBinary::ArgObjectTypeSpecialized::Vme:
        dstKernelInfoArg.offsetVmeMbBlockType = getOffset(src.metadataSpecialized.vme.mbBlockType);
        dstKernelInfoArg.offsetVmeSubpixelMode = getOffset(src.metadataSpecialized.vme.subpixelMode);
        dstKernelInfoArg.offsetVmeSadAdjustMode = getOffset(src.metadataSpecialized.vme.sadAdjustMode);
        dstKernelInfoArg.offsetVmeSearchPathType = getOffset(src.metadataSpecialized.vme.searchPathType);
        break;
    }

    for (auto &byValArg : src.byValMap) {
        dstKernelInfo.storeKernelArgument(byValArg);
        if (byValArg->Type == DATA_PARAMETER_KERNEL_ARGUMENT) {
            dstKernelInfo.patchInfo.dataParameterBuffersKernelArgs.push_back(byValArg);
        }
    }

    dstKernelInfoArg.offsetObjectId = getOffset(src.objectId);
}

void populateKernelInfo(KernelInfo &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes) {
    UNRECOVERABLE_IF(nullptr == src.header);
    dst.heapInfo.pKernelHeader = src.header;
    dst.name = std::string(src.name.begin(), src.name.end()).c_str();
    dst.heapInfo.pKernelHeap = src.isa.begin();
    dst.heapInfo.pGsh = src.heaps.generalState.begin();
    dst.heapInfo.pDsh = src.heaps.dynamicState.begin();
    dst.heapInfo.pSsh = src.heaps.surfaceState.begin();

    storeTokenIfNotNull(dst, src.tokens.executionEnvironment);
    dst.patchInfo.samplerStateArray = src.tokens.samplerStateArray;
    dst.patchInfo.bindingTableState = src.tokens.bindingTableState;
    dst.usesSsh = src.tokens.bindingTableState && (src.tokens.bindingTableState->Count > 0);
    dst.patchInfo.localsurface = src.tokens.allocateLocalSurface;
    dst.workloadInfo.slmStaticSize = src.tokens.allocateLocalSurface ? src.tokens.allocateLocalSurface->TotalInlineLocalMemorySize : 0U;
    dst.patchInfo.mediavfestate = src.tokens.mediaVfeState[0];
    dst.patchInfo.mediaVfeStateSlot1 = src.tokens.mediaVfeState[1];
    dst.patchInfo.interfaceDescriptorDataLoad = src.tokens.mediaInterfaceDescriptorLoad;
    dst.patchInfo.interfaceDescriptorData = src.tokens.interfaceDescriptorData;
    dst.patchInfo.threadPayload = src.tokens.threadPayload;
    dst.patchInfo.dataParameterStream = src.tokens.dataParameterStream;

    dst.kernelArgInfo.resize(src.tokens.kernelArgs.size());

    for (size_t i = 0U; i < src.tokens.kernelArgs.size(); ++i) {
        auto &decodedKernelArg = src.tokens.kernelArgs[i];
        auto &kernelInfoArg = dst.kernelArgInfo[i];
        populateKernelInfoArg(dst, kernelInfoArg, decodedKernelArg);
    }

    storeTokenIfNotNull(dst, src.tokens.kernelAttributesInfo);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessPrivateSurface);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessConstantMemorySurfaceWithInitialization);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessPrintfSurface);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessEventPoolSurface);
    storeTokenIfNotNull(dst, src.tokens.allocateStatelessDefaultDeviceQueueSurface);
    storeTokenIfNotNull(dst, src.tokens.allocateSyncBuffer);

    for (auto &str : src.tokens.strings) {
        dst.storePatchToken(str);
    }

    dst.isVmeWorkload = dst.isVmeWorkload || (src.tokens.inlineVmeSamplerInfo != nullptr);
    dst.systemKernelOffset = src.tokens.stateSip ? src.tokens.stateSip->SystemKernelOffset : 0U;
    storeTokenIfNotNull(dst, src.tokens.allocateSystemThreadSurface);

    for (uint32_t i = 0; i < 3U; ++i) {
        dst.workloadInfo.localWorkSizeOffsets[i] = getOffset(src.tokens.crossThreadPayloadArgs.localWorkSize[i]);
        dst.workloadInfo.localWorkSizeOffsets2[i] = getOffset(src.tokens.crossThreadPayloadArgs.localWorkSize2[i]);
        dst.workloadInfo.globalWorkOffsetOffsets[i] = getOffset(src.tokens.crossThreadPayloadArgs.globalWorkOffset[i]);
        dst.workloadInfo.enqueuedLocalWorkSizeOffsets[i] = getOffset(src.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[i]);
        dst.workloadInfo.globalWorkSizeOffsets[i] = getOffset(src.tokens.crossThreadPayloadArgs.globalWorkSize[i]);
        dst.workloadInfo.numWorkGroupsOffset[i] = getOffset(src.tokens.crossThreadPayloadArgs.numWorkGroups[i]);
    }

    dst.workloadInfo.maxWorkGroupSizeOffset = getOffset(src.tokens.crossThreadPayloadArgs.maxWorkGroupSize);
    dst.workloadInfo.workDimOffset = getOffset(src.tokens.crossThreadPayloadArgs.workDimensions);
    dst.workloadInfo.simdSizeOffset = getOffset(src.tokens.crossThreadPayloadArgs.simdSize);
    dst.workloadInfo.parentEventOffset = getOffset(src.tokens.crossThreadPayloadArgs.parentEvent);
    dst.workloadInfo.preferredWkgMultipleOffset = getOffset(src.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple);
    dst.workloadInfo.privateMemoryStatelessSizeOffset = getOffset(src.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize);
    dst.workloadInfo.localMemoryStatelessWindowSizeOffset = getOffset(src.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize);
    dst.workloadInfo.localMemoryStatelessWindowStartAddressOffset = getOffset(src.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress);
    for (auto &childSimdSize : src.tokens.crossThreadPayloadArgs.childBlockSimdSize) {
        dst.childrenKernelsIdOffset.push_back({childSimdSize->ArgumentNumber, childSimdSize->Offset});
    }

    if (src.tokens.gtpinInfo) {
        dst.igcInfoForGtpin = reinterpret_cast<const gtpin::igc_info_t *>(src.tokens.gtpinInfo + 1);
    }

    dst.gpuPointerSize = gpuPointerSizeInBytes;

    if (dst.patchInfo.dataParameterStream && dst.patchInfo.dataParameterStream->DataParameterStreamSize) {
        uint32_t crossThreadDataSize = dst.patchInfo.dataParameterStream->DataParameterStreamSize;
        dst.crossThreadData = new char[crossThreadDataSize];
        memset(dst.crossThreadData, 0x00, crossThreadDataSize);
    }

    if (useKernelDescriptor) {
        populateKernelDescriptor(dst.kernelDescriptor, src, gpuPointerSizeInBytes);
    }
}

} // namespace NEO
