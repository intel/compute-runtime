/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/image_transformer.h"
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
            auto clMemObj = *(static_cast<const cl_mem *>(kernelArguments.at(argIndex).value));
            auto image = castToObjectOrAbort<Image>(clMemObj);
            auto surfaceState = ptrOffset(ssh, kernelInfo.kernelArgInfo.at(argIndex).offsetHeap);
            image->transformImage3dTo2dArray(surfaceState);
        }
    }
    transformed = true;
}
void ImageTransformer::transformImagesTo3d(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh) {
    for (auto const &argIndex : argIndexes) {
        auto clMemObj = *(static_cast<const cl_mem *>(kernelArguments.at(argIndex).value));
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
