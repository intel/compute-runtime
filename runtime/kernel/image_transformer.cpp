/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/image_transformer.h"

#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/image.h"
#include "runtime/program/kernel_info.h"

namespace OCLRT {
void ImageTransformer::registerImage3d(uint32_t argIndex) {
    if (std::find(argIndexes.begin(), argIndexes.end(), argIndex) == argIndexes.end()) {
        argIndexes.push_back(argIndex);
    }
}
void ImageTransformer::transformImagesTo2dArray(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh) {
    for (auto const &argIndex : argIndexes) {
        if (kernelInfo.kernelArgInfo.at(argIndex).isTransformable) {
            auto clMemObj = static_cast<cl_mem>(kernelArguments.at(argIndex).object);
            auto image = castToObjectOrAbort<Image>(clMemObj);
            auto surfaceState = ptrOffset(ssh, kernelInfo.kernelArgInfo.at(argIndex).offsetHeap);
            image->transformImage3dTo2dArray(surfaceState);
        }
    }
    transformed = true;
}
void ImageTransformer::transformImagesTo3d(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh) {
    for (auto const &argIndex : argIndexes) {
        auto clMemObj = static_cast<cl_mem>(kernelArguments.at(argIndex).object);
        auto image = castToObjectOrAbort<Image>(clMemObj);
        auto surfaceState = ptrOffset(ssh, kernelInfo.kernelArgInfo.at(argIndex).offsetHeap);
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
} // namespace OCLRT
