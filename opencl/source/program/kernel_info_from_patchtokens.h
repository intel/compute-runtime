/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct KernelInfo;

namespace PatchTokenBinary {
struct KernelFromPatchtokens;
}

void populateKernelInfo(KernelInfo &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes);

} // namespace NEO
