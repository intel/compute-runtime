/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {

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
                    this->bindlessArgsMap.emplace(std::pair{argImage.bindless, index});
                    index += 2;
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
