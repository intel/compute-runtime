/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/source/cmdlist/pnext_helpers.h"
#include "level_zero/driver_experimental/zex_graph.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {
void CommandList::setAdditionalDispatchKernelArgsFromLaunchParams(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const CmdListKernelLaunchParams &launchParams) const {
}

void CommandList::setAdditionalDispatchKernelArgsFromKernel(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const Kernel *kernel) const {
}

bool CommandList::canSkipImplicitInOrderDependency(CmdListKernelLaunchParams &launchParams, const Kernel &kernel) {
    return false;
}

ze_result_t CommandList::validateLaunchParams(const Kernel &kernel, const CmdListKernelLaunchParams &launchParams, const ze_group_count_t &threadGroupDimensions) const {
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::cloneAppendKernelExtensions(const ze_base_desc_t *desc, void *&outPnext) {
    outPnext = nullptr;
    ze_base_desc_t *chainedDesc = nullptr;

    while (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            cloneDescriptorHandler<ze_command_list_append_launch_kernel_param_cooperative_desc_t>(desc, outPnext, chainedDesc);
        } else if (desc->stype == ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC) {
            cloneDescriptorHandler<ze_graph_node_exp_desc_t>(desc, outPnext, chainedDesc);
        } else if (desc->stype == ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_DATA_EXP_DESC) {
            cloneDescriptorHandler<ze_mutable_kernel_data_exp_desc_t>(desc, outPnext, chainedDesc);
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        chainNextDescriptor(desc, chainedDesc);
    }

    return ZE_RESULT_SUCCESS;
}

void CommandList::freeClonedAppendKernelExtensions(void *pNext) {
    auto desc = reinterpret_cast<ze_base_desc_t *>(pNext);
    while (desc) {
        pNext = const_cast<void *>(desc->pNext);
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            auto cooperativeDesc = reinterpret_cast<ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(desc);
            delete cooperativeDesc;
        } else if (desc->stype == ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC) {
            auto graphNodeDesc = reinterpret_cast<ze_graph_node_exp_desc_t *>(desc);
            delete graphNodeDesc;
        } else if (desc->stype == ZE_STRUCTURE_TYPE_MUTABLE_KERNEL_DATA_EXP_DESC) {
            auto mutableKernelDataDesc = reinterpret_cast<ze_mutable_kernel_data_exp_desc_t *>(desc);
            delete mutableKernelDataDesc;
        }
        desc = reinterpret_cast<ze_base_desc_t *>(pNext);
    }
}

ze_result_t CommandList::obtainLaunchParamsFromExtensions(const ze_base_desc_t *desc, CmdListKernelLaunchParams &launchParams) const {
    while (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC) {
            auto cooperativeDesc = reinterpret_cast<const ze_command_list_append_launch_kernel_param_cooperative_desc_t *>(desc);
            launchParams.isCooperative = cooperativeDesc->isCooperative;
        } else if (desc->stype == ZE_STRUCTURE_TYPE_GRAPH_NODE_EXP_DESC) {
            launchParams.isGraphNodePresent = true;
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided extension, stype: 0x%x.\n",
                         desc->stype);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        desc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
    }
    return ZE_RESULT_SUCCESS;
}

void CommandList::setAdditionalBlitPropertiesFromMemoryCopyParams(NEO::BlitProperties &blitProperties, const CmdListMemoryCopyParams &memoryCopyParams) const {
}

ze_result_t CommandList::obtainMemoryCopyParamsFromExtensions(const ze_base_desc_t *desc, CmdListMemoryCopyParams &memoryCopyParams, bool writesOnly) const {
    if (desc) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided extension, stype: 0x%x.\n",
                     desc->stype);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::obtainCustomOperationParamsFromExtensions(const ze_base_desc_t *desc, CmdListCustomOperationParams &customOperationParams) const {
    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided extension, stype: 0x%x.\n",
                 desc->stype);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t CommandList::cloneAppendMemoryCopyExtensions(const ze_base_desc_t *desc, void *&outPnext) {
    if (desc) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return ZE_RESULT_SUCCESS;
}

void CommandList::freeClonedAppendMemoryCopyExtensions(void *pNext) {
}

} // namespace L0
