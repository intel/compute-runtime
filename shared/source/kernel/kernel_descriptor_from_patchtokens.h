/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace iOpenCL {
struct SPatchKernelAttributesInfo;
}

namespace NEO {
struct KernelDescriptor;

namespace PatchTokenBinary {
struct KernelFromPatchtokens;
}

void populateKernelDescriptor(KernelDescriptor &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes);
void populateKernelDescriptor(KernelDescriptor &dst, const iOpenCL::SPatchKernelAttributesInfo &token);

} // namespace NEO
