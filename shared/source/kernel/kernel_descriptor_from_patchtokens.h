/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct KernelDescriptor;

namespace PatchTokenBinary {
struct KernelFromPatchtokens;
}

void populateKernelDescriptor(KernelDescriptor &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes);

} // namespace NEO
