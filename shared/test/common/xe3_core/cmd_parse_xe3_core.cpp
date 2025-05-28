/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"

using GenStruct = NEO::Xe3Core;
using GenGfxFamily = NEO::Xe3CoreFamily;

#include "shared/test/common/cmd_parse/cmd_parse_mem_fence.inl"
#include "shared/test/common/cmd_parse/cmd_parse_mem_set.inl"
#include "shared/test/common/cmd_parse/cmd_parse_resource_barrier.inl"
#include "shared/test/common/cmd_parse/cmd_parse_state_context_data_base_address.inl"
#include "shared/test/common/cmd_parse/cmd_parse_state_prefetch.inl"
#include "shared/test/common/cmd_parse/cmd_parse_system_mem_fence_address.inl"
#include "shared/test/common/cmd_parse/cmd_parse_xy_block_copy.inl"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using MI_MEM_FENCE = GenStruct::MI_MEM_FENCE;
using STATE_SYSTEM_MEM_FENCE_ADDRESS = GenStruct::STATE_SYSTEM_MEM_FENCE_ADDRESS;
using STATE_PREFETCH = GenStruct::STATE_PREFETCH;
using MEM_SET = GenStruct::MEM_SET;
using STATE_CONTEXT_DATA_BASE_ADDRESS = GenStruct::STATE_CONTEXT_DATA_BASE_ADDRESS;
using RESOURCE_BARRIER = GenStruct::RESOURCE_BARRIER;

template <>
size_t CmdParse<GenGfxFamily>::getAdditionalCommandLength(void *cmd) {
    {
        if (genCmdCast<MI_MEM_FENCE *>(cmd)) {
            return sizeof(MI_MEM_FENCE) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmd)) {
            return sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<STATE_PREFETCH *>(cmd)) {
            return sizeof(STATE_PREFETCH) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<MEM_SET *>(cmd)) {
            return sizeof(MEM_SET) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmd)) {
            return sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<RESOURCE_BARRIER *>(cmd)) {
            return sizeof(RESOURCE_BARRIER) / sizeof(uint32_t);
        }
    }

    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getAdditionalCommandName(void *cmd) {
    if (genCmdCast<MI_MEM_FENCE *>(cmd)) {
        return "MI_MEM_FENCE";
    }
    if (genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmd)) {
        return "STATE_SYSTEM_MEM_FENCE_ADDRESS";
    }
    if (genCmdCast<STATE_PREFETCH *>(cmd)) {
        return "STATE_PREFETCH";
    }
    if (genCmdCast<MEM_SET *>(cmd)) {
        return "MEM_SET";
    }
    if (genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmd)) {
        return "STATE_CONTEXT_DATA_BASE_ADDRESS";
    }

    return "UNKNOWN";
}

#include "shared/test/common/cmd_parse/cmd_parse_xe_hpg_and_later.inl"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/cmd_parse/hw_parse_base.inl"
#include "shared/test/common/cmd_parse/hw_parse_xe2_hpg_and_later.inl"
#include "shared/test/common/cmd_parse/hw_parse_xe_hpg_and_later.inl"

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);
