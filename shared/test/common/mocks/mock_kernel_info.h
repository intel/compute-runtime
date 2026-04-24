/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/program/kernel_info.h"

#include <array>

namespace NEO {

class MockKernelInfo : public KernelInfo {
  public:
    void addArgBuffer(uint32_t index, CrossThreadDataOffset stateless = undefined<CrossThreadDataOffset>, uint8_t pointerSize = 0, SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>, CrossThreadDataOffset bindless = undefined<CrossThreadDataOffset>);
    void addArgImage(uint32_t index, SurfaceStateHeapOffset offset = undefined<CrossThreadDataOffset>, uint32_t type = static_cast<uint32_t>(NEOImageType::imageType2D), bool isTransformable = false);
    void addArgImmediate(uint32_t index, uint16_t size = 0, CrossThreadDataOffset offset = undefined<CrossThreadDataOffset>, uint16_t sourceOffset = 0, bool isDataParameterKernelArgument = false);
    void addArgLocal(uint32_t index, CrossThreadDataOffset slmOffset, uint8_t requiredSlmAlignment);
    void addArgPipe(uint32_t index, CrossThreadDataOffset stateless = undefined<CrossThreadDataOffset>, uint8_t pointerSize = 0, SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>);
    void addArgSampler(uint32_t index, SurfaceStateHeapOffset bindful = undefined<SurfaceStateHeapOffset>,
                       CrossThreadDataOffset addressingMode = undefined<CrossThreadDataOffset>, CrossThreadDataOffset normalizedCoords = undefined<CrossThreadDataOffset>, CrossThreadDataOffset snapWa = undefined<CrossThreadDataOffset>);

    inline ArgDescriptor &argAt(uint32_t index) { return kernelDescriptor.payloadMappings.explicitArgs[index]; }
    inline ArgDescPointer &argAsPtr(uint32_t index) { return argAt(index).as<ArgDescPointer>(); }
    inline ArgDescImage &argAsImg(uint32_t index) { return argAt(index).as<ArgDescImage>(); }
    inline ArgDescSampler &argAsSmp(uint32_t index) { return argAt(index).as<ArgDescSampler>(); }
    inline ArgDescValue &argAsVal(uint32_t index) { return argAt(index).as<ArgDescValue>(); }

    inline void setAddressQualifier(uint32_t index, uint8_t addressQualifier) { argAt(index).getTraits().addressQualifier = addressQualifier; }
    inline void setAccessQualifier(uint32_t index, uint8_t accessQualifier) { argAt(index).getTraits().accessQualifier = accessQualifier; }
    inline void setArgByValSize(uint32_t index, uint16_t argByValSize) { argAt(index).getTraits().argByValSize = argByValSize; }
    inline void setTypeQualifier(uint32_t index, KernelArgMetadata::TypeQualifiers typeQualifiers) { argAt(index).getTraits().typeQualifiers = typeQualifiers; }

    inline void setBufferStateful(uint32_t index, bool stateful = true) { argAsPtr(index).accessedUsingStatelessAddressingMode = !stateful; }

    inline void setBufferAddressingMode(KernelDescriptor::AddressingMode addressingMode) { kernelDescriptor.kernelAttributes.bufferAddressingMode = addressingMode; }
    inline void setImageAddressingMode(KernelDescriptor::AddressingMode addressingMode) { kernelDescriptor.kernelAttributes.imageAddressingMode = addressingMode; }

    inline void setCrossThreadDataSize(uint16_t crossThreadDataSize) { kernelDescriptor.kernelAttributes.crossThreadDataSize = crossThreadDataSize; }

    void addExtendedMetadata(uint32_t index, const std::string &argName = "", const std::string &type = "", const std::string &accessQualifier = "", const std::string &addressQualifier = "", const std::string &typeQualifiers = "", size_t typeSize = 0u);

    void setPrintfSurface(uint8_t dataParamSize = sizeof(uintptr_t), CrossThreadDataOffset crossThreadDataOffset = undefined<CrossThreadDataOffset>, SurfaceStateHeapOffset sshOffset = undefined<SurfaceStateHeapOffset>);
    void setBindingTable(SurfaceStateHeapOffset tableOffset, uint8_t numEntries);
    void setGlobalVariablesSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful = undefined<CrossThreadDataOffset>);
    void setDeviceSideEnqueueDefaultQueueSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful = undefined<CrossThreadDataOffset>);
    void setGlobalConstantsSurface(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful = undefined<CrossThreadDataOffset>);
    void setSyncBuffer(uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful = undefined<CrossThreadDataOffset>);
    void setPrivateMemory(uint32_t perThreadPrivateMemorySize, bool isSimtThread, uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful = undefined<CrossThreadDataOffset>);
    void setSamplerTable(DynamicStateHeapOffset borderColor, uint8_t numSamplers, DynamicStateHeapOffset tableOffset);
    void setLocalIds(const std::array<uint8_t, 3> &localIds);

  private:
    void populatePointerArg(ArgDescPointer &arg, uint8_t pointerSize, CrossThreadDataOffset stateless, SurfaceStateHeapOffset bindful);

    void resizeArgsIfIndexTooBig(uint32_t index);
};
} // namespace NEO
