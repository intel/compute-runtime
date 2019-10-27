/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct KernelInfo;

namespace PatchTokenBinary {
struct KernelFromPatchtokens;
}

void populateKernelInfo(KernelInfo &dst, const PatchTokenBinary::KernelFromPatchtokens &src);

} // namespace NEO
