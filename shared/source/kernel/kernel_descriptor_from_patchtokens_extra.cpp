/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

namespace NEO {
void populateKernelDescriptorExtra(KernelDescriptor &dst, const iOpenCL::SPatchExecutionEnvironment &execEnv) {
}
} // namespace NEO