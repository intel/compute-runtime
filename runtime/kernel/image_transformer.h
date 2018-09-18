/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/kernel/kernel.h"

namespace OCLRT {

class ImageTransformer {
  public:
    void registerImage3d(uint32_t argIndex);
    void transformImagesTo2dArray(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh);
    void transformImagesTo3d(const KernelInfo &kernelInfo, const std::vector<Kernel::SimpleKernelArgInfo> &kernelArguments, void *ssh);
    bool didTransform() const;
    bool hasRegisteredImages3d() const;

  protected:
    bool transformed = false;
    std::vector<uint32_t> argIndexes;
};
} // namespace OCLRT
