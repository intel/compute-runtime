/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/image_transformer.h"

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/program/kernel_info.h"

#include "opencl/source/mem_obj/image.h"

namespace NEO {
void ImageTransformer::registerImage3d(uint32_t argIndex) {
    if (std::find(argIndexes.begin(), argIndexes.end(), argIndex) == argIndexes.end()) {
        argIndexes.push_back(argIndex);
    }
}
void ImageTransformer::transformImagesTo2dArray(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh) {
    for (auto const &argIndex : argIndexes) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
        if (arg.getExtendedTypeInfo().isTransformable) {
            auto clMemObj = static_cast<cl_mem>(kernelArguments.at(argIndex).object);
            auto image = castToObjectOrAbort<Image>(clMemObj);
            auto surfaceState = ptrOffset(ssh, arg.as<ArgDescImage>().bindful);
            image->transformImage3dTo2dArray(surfaceState);
        }
    }
    transformed = true;
}
void ImageTransformer::transformImagesTo3d(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh) {
    for (auto const &argIndex : argIndexes) {
        const auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[argIndex];
        auto clMemObj = static_cast<cl_mem>(kernelArguments.at(argIndex).object);
        auto image = castToObjectOrAbort<Image>(clMemObj);
        auto surfaceState = ptrOffset(ssh, arg.as<ArgDescImage>().bindful);
        image->transformImage2dArrayTo3d(surfaceState);
    }
    transformed = false;
}
bool ImageTransformer::didTransform() const {
    return transformed;
}
bool ImageTransformer::hasRegisteredImages3d() const {
    return !argIndexes.empty();
}
} // namespace NEO
