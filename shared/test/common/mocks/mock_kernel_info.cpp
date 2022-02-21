/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_kernel_info.h"

#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

using namespace NEO;
namespace NEO {
void populatePointerKernelArg(ArgDescPointer &dst,
                              CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful, CrossThreadDataOffset bindless,
                              KernelDescriptor::AddressingMode addressingMode);
}

void MockKernelInfo::addArgAccelerator(uint32_t index, SurfaceStateHeapOffset bindful,
                                       CrossThreadDataOffset mbBlockType, CrossThreadDataOffset sadAdjustMode,
                                       CrossThreadDataOffset searchPathType, CrossThreadDataOffset subpixelMode) {
    addArgSampler(index, bindful);
    argAsSmp(index).samplerType = iOpenCL::SAMPLER_OBJECT_VME;
    argAt(index).getExtendedTypeInfo().isAccelerator = true;
    addExtendedVmeDescriptor(index, mbBlockType, sadAdjustMode, searchPathType, subpixelMode);
    kernelDescriptor.kernelAttributes.flags.usesVme = true;
}

void MockKernelInfo::addArgBuffer(uint32_t index, CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful, CrossThreadDataOffset bindless) {
    resizeArgsIfIndexTooBig(index);

    populatePointerKernelArg(argAt(index).as<ArgDescPointer>(true), stateless, pointerSize, bindful, bindless, kernelDescriptor.kernelAttributes.bufferAddressingMode);
}

void MockKernelInfo::addArgDevQueue(uint32_t index, CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful) {
    addArgBuffer(index, stateless, pointerSize, bindful);
    argAt(index).getExtendedTypeInfo().isDeviceQueue = true;
}

void MockKernelInfo::addArgImage(uint32_t index, SurfaceStateHeapOffset offset, uint32_t type, bool isTransformable) {
    resizeArgsIfIndexTooBig(index);

    auto &arg = argAt(index).as<ArgDescImage>(true);
    if (KernelDescriptor::Bindful == kernelDescriptor.kernelAttributes.imageAddressingMode) {
        arg.bindful = offset;
    } else if (KernelDescriptor::Bindless == kernelDescriptor.kernelAttributes.imageAddressingMode) {
        arg.bindless = offset;
    }

    if (type == iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA) {
        argAt(index).getExtendedTypeInfo().isMediaImage = true;
    } else if (type == iOpenCL::IMAGE_MEMORY_OBJECT_2D_MEDIA_BLOCK) {
        argAt(index).getExtendedTypeInfo().isMediaBlockImage = true;
    }

    argAt(index).getExtendedTypeInfo().isTransformable = isTransformable;

    kernelDescriptor.kernelAttributes.flags.usesImages = true;
}

void MockKernelInfo::addArgImmediate(uint32_t index, uint16_t size, CrossThreadDataOffset offset, uint16_t sourceOffset, bool isDataParameterKernelArgument) {
    resizeArgsIfIndexTooBig(index);

    ArgDescValue::Element element{offset, size, sourceOffset};
    argAt(index).as<ArgDescValue>(true).elements.push_back(element);

    if (isDataParameterKernelArgument) {
        kernelDescriptor.kernelMetadata.allByValueKernelArguments.push_back({element, static_cast<uint16_t>(index)});
    }
}

void MockKernelInfo::addArgLocal(uint32_t index, CrossThreadDataOffset slmOffset, uint8_t requiredSlmAlignment) {
    resizeArgsIfIndexTooBig(index);

    auto &arg = argAt(index).as<ArgDescPointer>(true);
    arg.slmOffset = slmOffset;
    arg.requiredSlmAlignment = requiredSlmAlignment;

    setAddressQualifier(index, KernelArgMetadata::AddrLocal);
}

void MockKernelInfo::addArgPipe(uint32_t index, CrossThreadDataOffset stateless, uint8_t pointerSize, SurfaceStateHeapOffset bindful) {
    addArgBuffer(index, stateless, pointerSize, bindful);
    argAt(index).getTraits().typeQualifiers.pipeQual = true;
}

void MockKernelInfo::addArgSampler(uint32_t index, SurfaceStateHeapOffset bindful,
                                   CrossThreadDataOffset addressingMode, CrossThreadDataOffset normalizedCoords, CrossThreadDataOffset snapWa) {
    resizeArgsIfIndexTooBig(index);

    auto &arg = argAt(index).as<ArgDescSampler>(true);
    arg.bindful = bindful;
    arg.samplerType = iOpenCL::SAMPLER_OBJECT_TEXTURE;

    arg.metadataPayload.samplerAddressingMode = addressingMode;
    arg.metadataPayload.samplerNormalizedCoords = normalizedCoords;
    arg.metadataPayload.samplerSnapWa = snapWa;
}

void MockKernelInfo::resizeArgsIfIndexTooBig(uint32_t index) {
    auto &args = kernelDescriptor.payloadMappings.explicitArgs;
    if (index >= args.size()) {
        args.resize(index + 1);
    }
}

void MockKernelInfo::addExtendedMetadata(uint32_t index, const std::string &argName, const std::string &type, const std::string &accessQualifier, const std::string &addressQualifier, const std::string &typeQualifiers) {
    auto &extendedMetadata = kernelDescriptor.explicitArgsExtendedMetadata;
    if (index >= extendedMetadata.size()) {
        extendedMetadata.resize(index + 1);
    }

    extendedMetadata[index] = {argName, type, accessQualifier, addressQualifier, typeQualifiers};
}

void MockKernelInfo::addExtendedDeviceSideEnqueueDescriptor(uint32_t index, uint32_t objectId) {
    auto &explicitArgsExtendedDescriptors = kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
    auto &explicitArgs = kernelDescriptor.payloadMappings.explicitArgs;
    if (index >= explicitArgsExtendedDescriptors.size()) {
        explicitArgsExtendedDescriptors.resize(index + 1);
    }

    auto deviceSideEnqueueDescriptor = std::make_unique<ArgDescriptorDeviceSideEnqueue>();
    deviceSideEnqueueDescriptor->objectId = objectId;
    explicitArgsExtendedDescriptors[index] = std::move(deviceSideEnqueueDescriptor);

    explicitArgs[index].getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor = true;
}

void MockKernelInfo::addExtendedVmeDescriptor(uint32_t index, CrossThreadDataOffset mbBlockType, CrossThreadDataOffset sadAdjustMode, CrossThreadDataOffset searchPathType, CrossThreadDataOffset subpixelMode) {
    auto &explicitArgsExtendedDescriptors = kernelDescriptor.payloadMappings.explicitArgsExtendedDescriptors;
    if (index >= explicitArgsExtendedDescriptors.size()) {
        explicitArgsExtendedDescriptors.resize(index + 1);
    }

    auto vmeDescriptor = std::make_unique<ArgDescVme>();
    vmeDescriptor->mbBlockType = mbBlockType;
    vmeDescriptor->sadAdjustMode = sadAdjustMode;
    vmeDescriptor->searchPathType = searchPathType;
    vmeDescriptor->subpixelMode = subpixelMode;
    explicitArgsExtendedDescriptors[index] = std::move(vmeDescriptor);

    argAt(index).getExtendedTypeInfo().hasVmeExtendedDescriptor = true;
}

void MockKernelInfo::populatePointerArg(ArgDescPointer &arg, uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    populatePointerKernelArg(arg, stateless, pointerSize, bindful, bindful, kernelDescriptor.kernelAttributes.bufferAddressingMode);
}

void MockKernelInfo::setPrintfSurface(uint8_t dataParamSize, CrossThreadDataOffset crossThreadDataOffset, SurfaceStateHeapOffset sshOffset) {
    kernelDescriptor.kernelAttributes.flags.usesPrintf = true;
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress, dataParamSize, crossThreadDataOffset, sshOffset);
}

void MockKernelInfo::setBindingTable(SurfaceStateHeapOffset tableOffset, uint8_t numEntries) {
    auto &bt = kernelDescriptor.payloadMappings.bindingTable;
    bt.tableOffset = tableOffset;
    bt.numEntries = numEntries;
}

void MockKernelInfo::setGlobalVariablesSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setDeviceSideEnqueueEventPoolSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueEventPoolSurfaceAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setDeviceSideEnqueueDefaultQueueSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setGlobalConstantsSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setSyncBuffer(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    kernelDescriptor.kernelAttributes.flags.usesSyncBuffer = true;
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setPrivateMemory(uint32_t perThreadPrivateMemorySize, bool isSimtThread, uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful) {
    kernelDescriptor.kernelAttributes.flags.usesPrivateMemory = true;
    auto multiplier = static_cast<uint32_t>(isSimtThread ? kernelDescriptor.kernelAttributes.simdSize : 1U);
    kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize = multiplier * perThreadPrivateMemorySize;
    populatePointerArg(kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress, pointerSize, stateless, bindful);
}

void MockKernelInfo::setSamplerTable(DynamicStateHeapOffset borderColor, uint8_t numSamplers, DynamicStateHeapOffset tableOffset) {
    auto &samplerTable = kernelDescriptor.payloadMappings.samplerTable;
    samplerTable.borderColor = borderColor;
    samplerTable.numSamplers = numSamplers;
    samplerTable.tableOffset = tableOffset;
}

void MockKernelInfo::setPerThreadScratchSize(uint32_t perThreadScratchSize, uint32_t slot) {
    kernelDescriptor.kernelAttributes.perThreadScratchSize[slot] = perThreadScratchSize;
}
void MockKernelInfo::setLocalIds(const std::array<uint8_t, 3> &localIds) {
    kernelDescriptor.kernelAttributes.numLocalIdChannels = localIds[0] + localIds[1] + localIds[2];
    kernelDescriptor.kernelAttributes.localId[0] = localIds[0];
    kernelDescriptor.kernelAttributes.localId[1] = localIds[1];
    kernelDescriptor.kernelAttributes.localId[2] = localIds[2];
}
