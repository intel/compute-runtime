/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"

namespace L0 {
struct CmdListKernelLaunchParams;

void CommandList::setAdditionalDispatchKernelArgsFromLaunchParams(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const CmdListKernelLaunchParams &launchParams) const {
}

void CommandList::setAdditionalDispatchKernelArgsFromKernel(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const Kernel *kernel) const {
}

ze_result_t CommandList::validateLaunchParams(const Kernel &kernel, const CmdListKernelLaunchParams &launchParams) const {
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::obtainLaunchParamsFromExtensions(const ze_base_desc_t *desc, CmdListKernelLaunchParams &launchParams, ze_kernel_handle_t kernelHandle) const {
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
