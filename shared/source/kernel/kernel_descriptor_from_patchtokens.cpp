/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor.h"

#include <sstream>
#include <string>

namespace NEO {

using namespace iOpenCL;

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchExecutionEnvironment &execEnv) {
    if (execEnv.RequiredWorkGroupSizeX != 0) {
        dst.kernelAttributes.requiredWorkgroupSize[0] = execEnv.RequiredWorkGroupSizeX;
        dst.kernelAttributes.requiredWorkgroupSize[1] = execEnv.RequiredWorkGroupSizeY;
        dst.kernelAttributes.requiredWorkgroupSize[2] = execEnv.RequiredWorkGroupSizeZ;
        DEBUG_BREAK_IF(!(execEnv.RequiredWorkGroupSizeY > 0));
        DEBUG_BREAK_IF(!(execEnv.RequiredWorkGroupSizeZ > 0));
    }
    if (execEnv.WorkgroupWalkOrderDims) {
        constexpr auto dimensionMask = 0b11;
        constexpr auto dimensionSize = 2;
        dst.kernelAttributes.workgroupWalkOrder[0] = execEnv.WorkgroupWalkOrderDims & dimensionMask;
        dst.kernelAttributes.workgroupWalkOrder[1] = (execEnv.WorkgroupWalkOrderDims >> dimensionSize) & dimensionMask;
        dst.kernelAttributes.workgroupWalkOrder[2] = (execEnv.WorkgroupWalkOrderDims >> dimensionSize * 2) & dimensionMask;
        dst.kernelAttributes.flags.requiresWorkgroupWalkOrder = true;
    }

    for (uint32_t i = 0; i < 3; ++i) {
        // inverts the walk order mapping (from ORDER_ID->DIM_ID to DIM_ID->ORDER_ID)
        dst.kernelAttributes.workgroupDimensionsOrder[dst.kernelAttributes.workgroupWalkOrder[i]] = i;
    }

    if (execEnv.CompiledForGreaterThan4GBBuffers) {
        dst.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
    } else if (execEnv.UseBindlessMode) {
        dst.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;
        dst.kernelAttributes.imageAddressingMode = KernelDescriptor::Bindless;
    } else {
        dst.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;
    }

    dst.kernelAttributes.numGrfRequired = execEnv.NumGRFRequired;
    dst.kernelAttributes.simdSize = execEnv.LargestCompiledSIMDSize;
    dst.kernelAttributes.barrierCount = execEnv.HasBarriers;
    dst.kernelAttributes.numThreadsRequired = execEnv.NumThreadsRequired;

    dst.kernelAttributes.flags.requiresDisabledEUFusion = (0 != execEnv.RequireDisableEUFusion);
    dst.kernelAttributes.flags.requiresDisabledMidThreadPreemption = (0 != execEnv.DisableMidThreadPreemption);
    dst.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress = (0 != execEnv.SubgroupIndependentForwardProgressRequired);
    dst.kernelAttributes.flags.usesFencesForReadWriteImages = (0 != execEnv.UsesFencesForReadWriteImages);
    dst.kernelAttributes.flags.usesSystolicPipelineSelectMode = (0 != execEnv.HasDPAS);
    dst.kernelAttributes.flags.usesStatelessWrites = (0 != execEnv.StatelessWritesCount);
    dst.kernelAttributes.flags.useStackCalls = (0 != execEnv.HasStackCalls);
    dst.kernelAttributes.flags.hasRTCalls = (0 != execEnv.HasRTCalls);

    dst.kernelMetadata.compiledSubGroupsNumber = execEnv.CompiledSubGroupsNumber;

    populateKernelDescriptorExtra(dst, execEnv);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchSamplerStateArray &token) {
    dst.payloadMappings.samplerTable.borderColor = token.BorderColorOffset;
    dst.payloadMappings.samplerTable.numSamplers = token.Count;
    dst.payloadMappings.samplerTable.tableOffset = token.Offset;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchBindingTableState &token) {
    dst.payloadMappings.bindingTable.numEntries = token.Count;
    dst.payloadMappings.bindingTable.tableOffset = token.Offset;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateLocalSurface &token) {
    dst.kernelAttributes.slmInlineSize = token.TotalInlineLocalMemorySize;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchMediaVFEState &token, uint32_t slot) {
    UNRECOVERABLE_IF(slot >= 2U);
    dst.kernelAttributes.perThreadScratchSize[slot] = token.PerThreadScratchSpace;
    if (slot == 0) {
        dst.kernelAttributes.spillFillScratchMemorySize = token.PerThreadScratchSpace;
    } else if (slot == 1) {
        dst.kernelAttributes.privateScratchMemorySize = token.PerThreadScratchSpace;
    }
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchThreadPayload &token) {
    dst.kernelAttributes.flags.perThreadDataHeaderIsPresent = (0U != token.HeaderPresent);
    dst.kernelAttributes.numLocalIdChannels = token.LocalIDXPresent + token.LocalIDYPresent + token.LocalIDZPresent;
    dst.kernelAttributes.localId[0] = token.LocalIDXPresent;
    dst.kernelAttributes.localId[1] = token.LocalIDYPresent;
    dst.kernelAttributes.localId[2] = token.LocalIDZPresent;
    dst.kernelAttributes.flags.usesFlattenedLocalIds = (0U != token.LocalIDFlattenedPresent);
    dst.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent = (0U != token.UnusedPerThreadConstantPresent);
    dst.kernelAttributes.flags.passInlineData = (0 != token.PassInlineData);
    dst.entryPoints.skipPerThreadDataLoad = token.OffsetToSkipPerThreadDataLoad;
    dst.entryPoints.skipSetFFIDGP = token.OffsetToSkipSetFFIDGP;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchDataParameterStream &token) {
    dst.kernelAttributes.crossThreadDataSize = token.DataParameterStreamSize;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchKernelAttributesInfo &token) {
    constexpr ConstStringRef attributeReqdSubGroupSizeBeg = "intel_reqd_sub_group_size(";
    std::string attributes = std::string(reinterpret_cast<const char *>(&token + 1), token.AttributesSize).c_str();
    dst.kernelMetadata.kernelLanguageAttributes = attributes;
    auto it = attributes.find(attributeReqdSubGroupSizeBeg.begin());
    if (it != std::string::npos) {
        it += attributeReqdSubGroupSizeBeg.size();
        dst.kernelMetadata.requiredSubGroupSize = 0U;
        while ((attributes[it] >= '0') && (attributes[it] <= '9')) {
            dst.kernelMetadata.requiredSubGroupSize *= 10;
            dst.kernelMetadata.requiredSubGroupSize += attributes[it] - '0';
            ++it;
        }
    }

    constexpr ConstStringRef attributeReqdThreadGroupDispatchSizeBeg = "intel_reqd_thread_group_dispatch_size(";
    it = attributes.find(attributeReqdThreadGroupDispatchSizeBeg.begin());
    if (it != std::string::npos) {
        it += attributeReqdThreadGroupDispatchSizeBeg.size();
        dst.kernelMetadata.requiredThreadGroupDispatchSize = 0U;
        while ((attributes[it] >= '0') && (attributes[it] <= '9')) {
            dst.kernelMetadata.requiredThreadGroupDispatchSize *= 10;
            dst.kernelMetadata.requiredThreadGroupDispatchSize += attributes[it] - '0';
            ++it;
        }
    }

    constexpr ConstStringRef invalidKernelAttrBeg = "invalid_kernel(";
    dst.kernelAttributes.flags.isInvalid = (attributes.find(invalidKernelAttrBeg.data()) != std::string::npos);
}

void populatePointerKernelArg(KernelDescriptor &kernelDesc, ArgDescPointer &dst,
                              CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful, CrossThreadDataOffset bindless,
                              KernelDescriptor::AddressingMode addressingMode) {
    switch (addressingMode) {
    default:
        UNRECOVERABLE_IF(KernelDescriptor::Stateless != addressingMode);
        dst.bindful = undefined<SurfaceStateHeapOffset>;
        dst.stateless = stateless;
        dst.bindless = undefined<CrossThreadDataOffset>;
        dst.pointerSize = pointerSize;
        break;
    case KernelDescriptor::BindfulAndStateless:
        dst.bindful = bindful;
        dst.stateless = stateless;
        dst.bindless = undefined<CrossThreadDataOffset>;
        dst.pointerSize = pointerSize;
        kernelDesc.kernelAttributes.numArgsStateful++;
        break;

    case KernelDescriptor::BindlessAndStateless:
        dst.bindful = undefined<SurfaceStateHeapOffset>;
        dst.stateless = stateless;
        dst.bindless = bindless;
        dst.pointerSize = pointerSize;
        kernelDesc.kernelAttributes.numArgsStateful++;
        break;
    }
}

template <typename TokenT>
void populatePointerKernelArg(KernelDescriptor &kernelDesc, ArgDescPointer &dst, const TokenT &src, KernelDescriptor::AddressingMode addressingMode) {
    populatePointerKernelArg(kernelDesc, dst, src.DataParamOffset, src.DataParamSize, src.SurfaceStateHeapOffset, src.SurfaceStateHeapOffset, addressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessPrivateSurface &token) {
    dst.kernelAttributes.flags.usesPrivateMemory = true;
    dst.kernelAttributes.perHwThreadPrivateMemorySize = static_cast<uint32_t>(PatchTokenBinary::getPerHwThreadPrivateSurfaceSize(token, dst.kernelAttributes.simdSize));
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.privateMemoryAddress, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization &token) {
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.globalConstantsSurfaceAddress, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization &token) {
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.globalVariablesSurfaceAddress, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessPrintfSurface &token) {
    dst.kernelAttributes.flags.usesPrintf = true;
    dst.kernelAttributes.flags.usesStringMapForPrintf = true;
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.printfSurfaceAddress, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateSystemThreadSurface &token) {
    dst.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = token.Offset;
    dst.kernelAttributes.perThreadSystemThreadSurfaceSize = token.PerThreadSystemThreadSurfaceSize;
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateSyncBuffer &token) {
    dst.kernelAttributes.flags.usesSyncBuffer = true;
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.syncBufferAddress, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateRTGlobalBuffer &token) {
    populatePointerKernelArg(dst, dst.payloadMappings.implicitArgs.rtDispatchGlobals, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchString &token) {
    uint32_t stringIndex = token.Index;
    const char *stringData = reinterpret_cast<const char *>(&token + 1);
    dst.kernelMetadata.printfStringsMap[stringIndex].assign(stringData, stringData + token.StringSize);
}

template <typename TokenT, typename... ArgsT>
inline void populateKernelDescriptorIfNotNull(KernelDescriptor &dst, const TokenT *token, ArgsT &&...args) {
    if (token != nullptr) {
        populateKernelDescriptor(dst, *token, std::forward<ArgsT>(args)...);
    }
}

void markArgAsPatchable(KernelDescriptor &parent, size_t dstArgNum) {
    auto &argExtendedTypeInfo = parent.payloadMappings.explicitArgs[dstArgNum].getExtendedTypeInfo();
    if (false == argExtendedTypeInfo.needsPatch) {
        argExtendedTypeInfo.needsPatch = true;
        ++parent.kernelAttributes.numArgsToPatch;
    }
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchImageMemoryObjectKernelArgument &token) {
    markArgAsPatchable(dst, argNum);

    auto &argImage = dst.payloadMappings.explicitArgs[argNum].as<ArgDescImage>(true);
    if (KernelDescriptor::Bindful == dst.kernelAttributes.imageAddressingMode) {
        argImage.bindful = token.Offset;
        dst.kernelAttributes.numArgsStateful++;
    }
    if (KernelDescriptor::Bindless == dst.kernelAttributes.imageAddressingMode) {
        argImage.bindless = token.Offset;
        dst.kernelAttributes.numArgsStateful++;
    }

    if (token.Type == iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK) {
        dst.payloadMappings.explicitArgs[argNum].getExtendedTypeInfo().isMediaBlockImage = true;
    }

    dst.payloadMappings.explicitArgs[argNum].getExtendedTypeInfo().isTransformable = token.Transformable != 0;
    if (NEO::KernelArgMetadata::AccessUnknown == dst.payloadMappings.explicitArgs[argNum].getTraits().accessQualifier) {
        auto accessQual = token.Writeable ? NEO::KernelArgMetadata::AccessReadWrite
                                          : NEO::KernelArgMetadata::AccessReadOnly;
        dst.payloadMappings.explicitArgs[argNum].getTraits().accessQualifier = accessQual;
    }
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchSamplerKernelArgument &token) {
    markArgAsPatchable(dst, argNum);

    auto &argSampler = dst.payloadMappings.explicitArgs[argNum].as<ArgDescSampler>(true);

    argSampler.bindful = token.Offset;
    argSampler.samplerType = token.Type;
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchGlobalMemoryObjectKernelArgument &token) {
    markArgAsPatchable(dst, argNum);

    auto &argPointer = dst.payloadMappings.explicitArgs[argNum].as<ArgDescPointer>(true);
    dst.payloadMappings.explicitArgs[argNum].getTraits().addressQualifier = KernelArgMetadata::AddrGlobal;
    if (dst.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindlessAndStateless) {
        argPointer.bindless = token.Offset;
        argPointer.bindful = undefined<SurfaceStateHeapOffset>;
        dst.kernelAttributes.numArgsStateful++;
    } else {
        argPointer.bindful = token.Offset;
        argPointer.bindless = undefined<CrossThreadDataOffset>;
        dst.kernelAttributes.numArgsStateful++;
    }

    argPointer.stateless = undefined<CrossThreadDataOffset>;
    argPointer.pointerSize = dst.kernelAttributes.gpuPointerSize;
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchStatelessGlobalMemoryObjectKernelArgument &token) {
    markArgAsPatchable(dst, argNum);

    auto &argPointer = dst.payloadMappings.explicitArgs[argNum].as<ArgDescPointer>(true);
    dst.payloadMappings.explicitArgs[argNum].getTraits().addressQualifier = KernelArgMetadata::AddrGlobal;

    populatePointerKernelArg(dst, argPointer, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchStatelessConstantMemoryObjectKernelArgument &token) {
    markArgAsPatchable(dst, argNum);

    auto &argPointer = dst.payloadMappings.explicitArgs[argNum].as<ArgDescPointer>(true);
    dst.payloadMappings.explicitArgs[argNum].getTraits().addressQualifier = KernelArgMetadata::AddrConstant;

    populatePointerKernelArg(dst, argPointer, token, dst.kernelAttributes.bufferAddressingMode);
}

void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchDataParameterBuffer &token) {
    markArgAsPatchable(dst, argNum);

    ArgDescValue::Element newElement = {};
    newElement.size = token.DataSize;
    newElement.offset = token.Offset;
    newElement.sourceOffset = token.SourceOffset;

    dst.payloadMappings.explicitArgs[argNum].as<ArgDescValue>(true).elements.push_back(newElement);
}

inline CrossThreadDataOffset getOffset(const SPatchDataParameterBuffer *token) {
    if (token != nullptr) {
        return static_cast<CrossThreadDataOffset>(token->Offset);
    }
    return undefined<CrossThreadDataOffset>;
}

void populateArgMetadata(KernelDescriptor &dst, size_t argNum, const SPatchKernelArgumentInfo *src) {
    if (nullptr == src) {
        return;
    }

    auto inlineData = PatchTokenBinary::getInlineData(src);

    auto metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    metadataExtended->addressQualifier = parseLimitedString(inlineData.addressQualifier.begin(), inlineData.addressQualifier.size());
    metadataExtended->accessQualifier = parseLimitedString(inlineData.accessQualifier.begin(), inlineData.accessQualifier.size());
    metadataExtended->argName = parseLimitedString(inlineData.argName.begin(), inlineData.argName.size());
    metadataExtended->type = parseLimitedString(inlineData.typeName.begin(), inlineData.typeName.size());
    auto delimPosition = metadataExtended->type.find(";");
    if (delimPosition != std::string::npos) {
        metadataExtended->type = metadataExtended->type.substr(0, delimPosition);
    }
    metadataExtended->typeQualifiers = parseLimitedString(inlineData.typeQualifiers.begin(), inlineData.typeQualifiers.size());

    ArgTypeTraits metadata = {};
    metadata.accessQualifier = KernelArgMetadata::parseAccessQualifier(metadataExtended->accessQualifier);
    metadata.addressQualifier = KernelArgMetadata::parseAddressSpace(metadataExtended->addressQualifier);
    metadata.typeQualifiers = KernelArgMetadata::parseTypeQualifiers(metadataExtended->typeQualifiers);

    markArgAsPatchable(dst, argNum);

    dst.payloadMappings.explicitArgs[argNum].getTraits() = metadata;
    dst.explicitArgsExtendedMetadata[argNum] = std::move(*metadataExtended);
}

void populateArgDescriptor(KernelDescriptor &dst, size_t argNum, const PatchTokenBinary::KernelArgFromPatchtokens &src) {
    if (src.objectArg != nullptr) {
        switch (src.objectArg->Token) {
        default:
            UNRECOVERABLE_IF(PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT != src.objectArg->Token);
            populateKernelArgDescriptor(dst, argNum, *reinterpret_cast<const SPatchImageMemoryObjectKernelArgument *>(src.objectArg));
            dst.kernelAttributes.flags.usesImages = true;
            break;
        case PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT:
            populateKernelArgDescriptor(dst, argNum, *reinterpret_cast<const SPatchSamplerKernelArgument *>(src.objectArg));
            dst.kernelAttributes.flags.usesSamplers = true;
            break;
        case PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
            populateKernelArgDescriptor(dst, argNum, *reinterpret_cast<const SPatchGlobalMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
            populateKernelArgDescriptor(dst, argNum, *reinterpret_cast<const SPatchStatelessGlobalMemoryObjectKernelArgument *>(src.objectArg));
            break;
        case PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT:
            populateKernelArgDescriptor(dst, argNum, *reinterpret_cast<const SPatchStatelessConstantMemoryObjectKernelArgument *>(src.objectArg));
            break;
        }
    }

    switch (src.objectType) {
    default:
        UNRECOVERABLE_IF(PatchTokenBinary::ArgObjectType::none != src.objectType);
        break;
    case PatchTokenBinary::ArgObjectType::buffer: {
        auto &asBufferArg = dst.payloadMappings.explicitArgs[argNum].as<ArgDescPointer>(true);
        asBufferArg.bufferOffset = getOffset(src.metadata.buffer.bufferOffset);
        if (src.metadata.buffer.pureStateful != nullptr) {
            asBufferArg.accessedUsingStatelessAddressingMode = false;
        }
    } break;
    case PatchTokenBinary::ArgObjectType::image: {
        auto &asImageArg = dst.payloadMappings.explicitArgs[argNum].as<ArgDescImage>(true);
        asImageArg.metadataPayload.imgWidth = getOffset(src.metadata.image.width);
        asImageArg.metadataPayload.imgHeight = getOffset(src.metadata.image.height);
        asImageArg.metadataPayload.imgDepth = getOffset(src.metadata.image.depth);
        asImageArg.metadataPayload.channelDataType = getOffset(src.metadata.image.channelDataType);
        asImageArg.metadataPayload.channelOrder = getOffset(src.metadata.image.channelOrder);
        asImageArg.metadataPayload.arraySize = getOffset(src.metadata.image.arraySize);
        asImageArg.metadataPayload.numSamples = getOffset(src.metadata.image.numSamples);
        asImageArg.metadataPayload.numMipLevels = getOffset(src.metadata.image.numMipLevels);
        asImageArg.metadataPayload.flatBaseOffset = getOffset(src.metadata.image.flatBaseOffset);
        asImageArg.metadataPayload.flatWidth = getOffset(src.metadata.image.flatWidth);
        asImageArg.metadataPayload.flatHeight = getOffset(src.metadata.image.flatHeight);
        asImageArg.metadataPayload.flatPitch = getOffset(src.metadata.image.flatPitch);
        dst.kernelAttributes.flags.usesImages = true;
    } break;
    case PatchTokenBinary::ArgObjectType::sampler: {
        auto &asSamplerArg = dst.payloadMappings.explicitArgs[argNum].as<ArgDescSampler>(true);
        asSamplerArg.metadataPayload.samplerSnapWa = getOffset(src.metadata.sampler.coordinateSnapWaRequired);
        asSamplerArg.metadataPayload.samplerAddressingMode = getOffset(src.metadata.sampler.addressMode);
        asSamplerArg.metadataPayload.samplerNormalizedCoords = getOffset(src.metadata.sampler.normalizedCoords);
        dst.kernelAttributes.flags.usesSamplers = true;
    } break;
    case PatchTokenBinary::ArgObjectType::slm: {
        markArgAsPatchable(dst, argNum);
        auto &asBufferArg = dst.payloadMappings.explicitArgs[argNum].as<ArgDescPointer>(true);
        asBufferArg.requiredSlmAlignment = src.metadata.slm.token->SourceOffset;
        asBufferArg.slmOffset = src.metadata.slm.token->Offset;
    } break;
    }

    for (auto &byValArg : src.byValMap) {
        if (PatchTokenBinary::ArgObjectType::slm != src.objectType) {
            populateKernelArgDescriptor(dst, argNum, *byValArg);
        }
    }
    populateArgMetadata(dst, argNum, src.argInfo);
}

void populateKernelDescriptor(KernelDescriptor &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes) {
    UNRECOVERABLE_IF(nullptr == src.header);
    dst.kernelMetadata.kernelName = std::string(src.name.begin(), src.name.end()).c_str();
    populateKernelDescriptorIfNotNull(dst, src.tokens.executionEnvironment);
    populateKernelDescriptorIfNotNull(dst, src.tokens.samplerStateArray);
    populateKernelDescriptorIfNotNull(dst, src.tokens.bindingTableState);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateLocalSurface);
    populateKernelDescriptorIfNotNull(dst, src.tokens.mediaVfeState[0], 0);
    populateKernelDescriptorIfNotNull(dst, src.tokens.mediaVfeState[1], 1);
    populateKernelDescriptorIfNotNull(dst, src.tokens.threadPayload);
    populateKernelDescriptorIfNotNull(dst, src.tokens.dataParameterStream);
    populateKernelDescriptorIfNotNull(dst, src.tokens.kernelAttributesInfo);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateStatelessPrivateSurface);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateStatelessConstantMemorySurfaceWithInitialization);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateStatelessPrintfSurface);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateSyncBuffer);
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateRTGlobalBuffer);

    dst.payloadMappings.explicitArgs.resize(src.tokens.kernelArgs.size());
    dst.explicitArgsExtendedMetadata.resize(src.tokens.kernelArgs.size());

    for (size_t i = 0U; i < src.tokens.kernelArgs.size(); ++i) {
        auto &decodedKernelArg = src.tokens.kernelArgs[i];
        populateArgDescriptor(dst, i, decodedKernelArg);
    }

    for (auto &str : src.tokens.strings) {
        populateKernelDescriptorIfNotNull(dst, str);
    }

    dst.entryPoints.systemKernel = src.tokens.stateSip ? src.tokens.stateSip->SystemKernelOffset : 0U;
    populateKernelDescriptorIfNotNull(dst, src.tokens.allocateSystemThreadSurface);

    for (uint32_t i = 0; i < 3U; ++i) {
        dst.payloadMappings.dispatchTraits.localWorkSize[i] = getOffset(src.tokens.crossThreadPayloadArgs.localWorkSize[i]);
        dst.payloadMappings.dispatchTraits.localWorkSize2[i] = getOffset(src.tokens.crossThreadPayloadArgs.localWorkSize2[i]);
        dst.payloadMappings.dispatchTraits.globalWorkOffset[i] = getOffset(src.tokens.crossThreadPayloadArgs.globalWorkOffset[i]);
        dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i] = getOffset(src.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[i]);
        dst.payloadMappings.dispatchTraits.globalWorkSize[i] = getOffset(src.tokens.crossThreadPayloadArgs.globalWorkSize[i]);
        dst.payloadMappings.dispatchTraits.numWorkGroups[i] = getOffset(src.tokens.crossThreadPayloadArgs.numWorkGroups[i]);
    }
    dst.payloadMappings.dispatchTraits.workDim = getOffset(src.tokens.crossThreadPayloadArgs.workDimensions);

    dst.payloadMappings.implicitArgs.maxWorkGroupSize = getOffset(src.tokens.crossThreadPayloadArgs.maxWorkGroupSize);
    dst.payloadMappings.implicitArgs.simdSize = getOffset(src.tokens.crossThreadPayloadArgs.simdSize);
    dst.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent = getOffset(src.tokens.crossThreadPayloadArgs.parentEvent);
    dst.payloadMappings.implicitArgs.preferredWkgMultiple = getOffset(src.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple);
    dst.payloadMappings.implicitArgs.privateMemorySize = getOffset(src.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize);
    dst.payloadMappings.implicitArgs.localMemoryStatelessWindowSize = getOffset(src.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize);
    dst.payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres = getOffset(src.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress);
    dst.payloadMappings.implicitArgs.implicitArgsBuffer = getOffset(src.tokens.crossThreadPayloadArgs.implicitArgsBufferOffset);

    if (src.tokens.gtpinInfo) {
        dst.external.igcInfoForGtpin = (src.tokens.gtpinInfo + 1);
    }

    dst.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
    dst.kernelAttributes.gpuPointerSize = gpuPointerSizeInBytes;
    dst.kernelAttributes.flags.requiresImplicitArgs = src.tokens.crossThreadPayloadArgs.implicitArgsBufferOffset != nullptr;

    if (debugManager.flags.UpdateCrossThreadDataSize.get()) {
        dst.updateCrossThreadDataSize();
    }

    if (KernelDescriptor::isBindlessAddressingKernel(dst)) {
        dst.initBindlessOffsetToSurfaceState();
    }
}

} // namespace NEO
