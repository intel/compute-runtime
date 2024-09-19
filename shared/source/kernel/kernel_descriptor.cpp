/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor.h"

#include "shared/source/helpers/aligned_memory.h"

namespace NEO {

void KernelDescriptor::updateCrossThreadDataSize() {
    uint32_t crossThreadDataSize = 0;
    for (uint32_t i = 0; i < 3; i++) {
        if (isValidOffset(payloadMappings.dispatchTraits.globalWorkOffset[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.globalWorkOffset[i] + sizeof(uint32_t));
        }
        if (isValidOffset(payloadMappings.dispatchTraits.globalWorkSize[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.globalWorkSize[i] + sizeof(uint32_t));
        }
        if (isValidOffset(payloadMappings.dispatchTraits.localWorkSize[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.localWorkSize[i] + sizeof(uint32_t));
        }
        if (isValidOffset(payloadMappings.dispatchTraits.localWorkSize2[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.localWorkSize2[i] + sizeof(uint32_t));
        }
        if (isValidOffset(payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i] + sizeof(uint32_t));
        }
        if (isValidOffset(payloadMappings.dispatchTraits.numWorkGroups[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.numWorkGroups[i] + sizeof(uint32_t));
        }
    }

    if (isValidOffset(payloadMappings.dispatchTraits.workDim)) {
        crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, payloadMappings.dispatchTraits.workDim + sizeof(uint32_t));
    }

    StackVec<ArgDescPointer *, 8> implicitArgsVec({&payloadMappings.implicitArgs.printfSurfaceAddress,
                                                   &payloadMappings.implicitArgs.globalVariablesSurfaceAddress,
                                                   &payloadMappings.implicitArgs.globalConstantsSurfaceAddress,
                                                   &payloadMappings.implicitArgs.privateMemoryAddress,
                                                   &payloadMappings.implicitArgs.deviceSideEnqueueDefaultQueueSurfaceAddress,
                                                   &payloadMappings.implicitArgs.systemThreadSurfaceAddress,
                                                   &payloadMappings.implicitArgs.syncBufferAddress});

    for (size_t i = 0; i < implicitArgsVec.size(); i++) {
        if (isValidOffset(implicitArgsVec[i]->bindless)) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, implicitArgsVec[i]->bindless + sizeof(uint32_t));
        }

        if (isValidOffset(implicitArgsVec[i]->stateless)) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, implicitArgsVec[i]->stateless + implicitArgsVec[i]->pointerSize);
        }
    }

    StackVec<CrossThreadDataOffset *, 7> implicitArgsVec2({&payloadMappings.implicitArgs.privateMemorySize,
                                                           &payloadMappings.implicitArgs.maxWorkGroupSize,
                                                           &payloadMappings.implicitArgs.simdSize,
                                                           &payloadMappings.implicitArgs.deviceSideEnqueueParentEvent,
                                                           &payloadMappings.implicitArgs.preferredWkgMultiple,
                                                           &payloadMappings.implicitArgs.localMemoryStatelessWindowSize,
                                                           &payloadMappings.implicitArgs.localMemoryStatelessWindowStartAddres});

    for (size_t i = 0; i < implicitArgsVec2.size(); i++) {
        if (isValidOffset(*implicitArgsVec2[i])) {
            crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, *implicitArgsVec2[i] + sizeof(uint32_t));
        }
    }

    for (size_t i = 0; i < payloadMappings.explicitArgs.size(); i++) {

        switch (payloadMappings.explicitArgs[i].type) {
        case ArgDescriptor::ArgType::argTImage: {
            auto &argImage = payloadMappings.explicitArgs[i].as<ArgDescImage>(false);
            if (isValidOffset(argImage.bindless)) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argImage.bindless + sizeof(uint32_t));
            }
        } break;
        case ArgDescriptor::ArgType::argTPointer: {
            auto &argPtr = payloadMappings.explicitArgs[i].as<ArgDescPointer>(false);
            if (isValidOffset(argPtr.bindless)) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argPtr.bindless + sizeof(uint32_t));
            }
            if (isValidOffset(argPtr.stateless)) {
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argPtr.stateless + argPtr.pointerSize);
            }
        } break;
        case ArgDescriptor::ArgType::argTSampler: {
            auto &argSampler = payloadMappings.explicitArgs[i].as<ArgDescSampler>(false);
            UNRECOVERABLE_IF(isValidOffset(argSampler.bindless));
        } break;
        case ArgDescriptor::ArgType::argTValue: {
            auto &argVal = payloadMappings.explicitArgs[i].as<ArgDescValue>(false);
            for (size_t i = 0; i < argVal.elements.size(); i++) {
                UNRECOVERABLE_IF(!isValidOffset(argVal.elements[i].offset));
                crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, argVal.elements[i].offset + argVal.elements[i].size);
            }
        } break;
        default:
            break;
        }
    }

    this->kernelAttributes.crossThreadDataSize = std::max<uint16_t>(this->kernelAttributes.crossThreadDataSize, static_cast<uint16_t>(alignUp(crossThreadDataSize, 32)));
}

bool KernelDescriptor::isBindlessAddressingKernel(const KernelDescriptor &desc) {
    bool bindlessBuffers = false;
    bool bindlessImages = false;
    if (desc.kernelAttributes.bufferAddressingMode == KernelDescriptor::Bindless || desc.kernelAttributes.bufferAddressingMode == KernelDescriptor::BindlessAndStateless) {
        bindlessBuffers = true;
    }
    if (desc.kernelAttributes.flags.usesImages && desc.kernelAttributes.imageAddressingMode == KernelDescriptor::Bindless) {
        bindlessImages = true;
    }

    return bindlessBuffers || bindlessImages;
}

void KernelDescriptor::initBindlessOffsetToSurfaceState() {
    std::call_once(initBindlessArgsMapOnce, [this]() {
        uint32_t index = 0;

        for (size_t i = 0; i < this->payloadMappings.explicitArgs.size(); i++) {

            switch (this->payloadMappings.explicitArgs[i].type) {
            case ArgDescriptor::ArgType::argTImage: {
                auto &argImage = this->payloadMappings.explicitArgs[i].as<ArgDescImage>();
                if (isValidOffset(argImage.bindless)) {
                    this->bindlessArgsMap.emplace(std::pair{argImage.bindless, index++});
                }
            } break;
            case ArgDescriptor::ArgType::argTPointer: {
                auto &argPtr = payloadMappings.explicitArgs[i].as<ArgDescPointer>();
                if (isValidOffset(argPtr.bindless)) {
                    this->bindlessArgsMap.emplace(std::pair{argPtr.bindless, index++});
                }
            } break;
            default:
                break;
            }
        }

        auto implicitArgsVec = getImplicitArgBindlessCandidatesVec();

        for (size_t i = 0; i < implicitArgsVec.size(); i++) {
            if (isValidOffset(implicitArgsVec[i]->bindless)) {
                this->bindlessArgsMap.emplace(std::pair{implicitArgsVec[i]->bindless, index++});
            }
        }
    });
}

} // namespace NEO