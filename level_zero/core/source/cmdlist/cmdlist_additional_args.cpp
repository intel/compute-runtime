/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {
void CommandList::setAdditionalDispatchKernelArgsFromLaunchParams(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const CmdListKernelLaunchParams &launchParams) const {
}

void CommandList::setAdditionalDispatchKernelArgsFromKernel(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const Kernel *kernel) const {
}

ze_result_t CommandList::validateLaunchParams(const Kernel &kernel, const CmdListKernelLaunchParams &launchParams) const {
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::cloneAppendKernelExtensions(const ze_base_desc_t *desc, void *&outPnext) {
    if (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            auto cooperativeDesc = reinterpret_cast<const ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(desc);
            auto cloneCooperativeDesc = new ze_command_list_append_launch_kernel_param_cooperative_desc_t;
            *cloneCooperativeDesc = *cooperativeDesc;
            cloneCooperativeDesc->pNext = nullptr;
            outPnext = cloneCooperativeDesc;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }
    return ZE_RESULT_SUCCESS;
}

void CommandList::freeClonedAppendKernelExtensions(void *pNext) {
    auto desc = reinterpret_cast<ze_base_desc_t *>(pNext);
    if (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            auto cooperativeDesc = reinterpret_cast<ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(desc);
            delete cooperativeDesc;
        }
    }
}

ze_result_t CommandList::obtainLaunchParamsFromExtensions(const ze_base_desc_t *desc, CmdListKernelLaunchParams &launchParams, ze_kernel_handle_t kernelHandle) const {
    while (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            auto cooperativeDesc = reinterpret_cast<const ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(desc);
            launchParams.isCooperative = cooperativeDesc->isCooperative;
        } else {
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided extension, stype: 0x%x.\n",
                               desc->stype);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        desc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
    }
    return ZE_RESULT_SUCCESS;
}

void CommandList::setAdditionalBlitPropertiesFromMemoryCopyParams(NEO::BlitProperties &blitProperties, const CmdListMemoryCopyParams &memoryCopyParams) const {
}

ze_result_t CommandList::obtainMemoryCopyParamsFromExtensions(const ze_base_desc_t *desc, CmdListMemoryCopyParams &memoryCopyParams) const {
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
