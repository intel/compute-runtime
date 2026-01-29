/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"

using GenStruct = NEO::Xe3pCore;
using GenGfxFamily = NEO::Xe3pCoreFamily;

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
using COMPUTE_WALKER_2 = GenStruct::COMPUTE_WALKER_2;
using RESOURCE_BARRIER = GenStruct::RESOURCE_BARRIER;
using MI_SEMAPHORE_WAIT_LEGACY = GenGfxFamily::MI_SEMAPHORE_WAIT_LEGACY;
using MI_SEMAPHORE_WAIT = GenGfxFamily::MI_SEMAPHORE_WAIT;

template <>
COMPUTE_WALKER_2 *genCmdCast<COMPUTE_WALKER_2 *>(void *buffer) {
    auto pCmd = reinterpret_cast<COMPUTE_WALKER_2 *>(buffer);

    return COMPUTE_WALKER_2::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   COMPUTE_WALKER_2::PIPELINE_COMPUTE == pCmd->TheStructure.Common.Pipeline &&
                   COMPUTE_WALKER_2::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND == pCmd->TheStructure.Common.ComputeCommandOpcode &&
                   COMPUTE_WALKER_2::CFE_SUBOPCODE_COMPUTE_WALKER_2 == pCmd->TheStructure.Common.CfeSubopcode
               ? pCmd
               : nullptr;
}

template <>
MI_SEMAPHORE_WAIT_LEGACY *genCmdCast<MI_SEMAPHORE_WAIT_LEGACY *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);

    return MI_SEMAPHORE_WAIT_LEGACY::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_SEMAPHORE_WAIT_LEGACY::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

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
        if (genCmdCast<COMPUTE_WALKER_2 *>(cmd)) {
            return sizeof(COMPUTE_WALKER_2) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<RESOURCE_BARRIER *>(cmd)) {
            return sizeof(RESOURCE_BARRIER) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<MI_SEMAPHORE_WAIT_LEGACY *>(cmd)) {
            return sizeof(MI_SEMAPHORE_WAIT_LEGACY) / sizeof(uint32_t);
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
    if (genCmdCast<COMPUTE_WALKER_2 *>(cmd)) {
        return "COMPUTE_WALKER_2";
    }
    return "UNKNOWN";
}
#include "shared/test/common/cmd_parse/cmd_parse_xe_hpg_and_later.inl"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/cmd_parse/hw_parse_base.inl"
#include "shared/test/common/cmd_parse/hw_parse_xe2_hpg_and_later.inl"
#include "shared/test/common/xe3p_core/hw_parse_xe3p.inl"

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);
