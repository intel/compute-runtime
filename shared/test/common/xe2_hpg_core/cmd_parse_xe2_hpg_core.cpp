/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
using GenStruct = NEO::Xe2HpgCore;
using GenGfxFamily = NEO::Xe2HpgCoreFamily;

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using MI_MEM_FENCE = GenStruct::MI_MEM_FENCE;
using STATE_SYSTEM_MEM_FENCE_ADDRESS = GenStruct::STATE_SYSTEM_MEM_FENCE_ADDRESS;
using STATE_PREFETCH = GenStruct::STATE_PREFETCH;
using MEM_SET = GenStruct::MEM_SET;
using STATE_CONTEXT_DATA_BASE_ADDRESS = GenStruct::STATE_CONTEXT_DATA_BASE_ADDRESS;

template <>
MI_MEM_FENCE *genCmdCast<MI_MEM_FENCE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_MEM_FENCE *>(buffer);

    return (0x0 == pCmd->TheStructure.Common.MiCommandSubOpcode &&
            0x9 == pCmd->TheStructure.Common.MiCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
STATE_SYSTEM_MEM_FENCE_ADDRESS *genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(buffer);

    return (0x1 == pCmd->TheStructure.Common.DwordLength &&
            0x9 == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x1 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubtype &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
STATE_PREFETCH *genCmdCast<STATE_PREFETCH *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    return (0x2 == pCmd->TheStructure.Common.DwordLength &&
            0x3 == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x0 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubtype &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
MEM_SET *genCmdCast<MEM_SET *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEM_SET *>(buffer);

    return (0x5 == pCmd->TheStructure.Common.DwordLength &&
            0x5B == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
            0x2 == pCmd->TheStructure.Common.Client)
               ? pCmd
               : nullptr;
}

template <>
STATE_CONTEXT_DATA_BASE_ADDRESS *genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(buffer);

    return (0x1 == pCmd->TheStructure.Common.DwordLength &&
            0xb == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x1 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubtype &&
            0x3 == pCmd->TheStructure.Common.CommandType)
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

#include "shared/test/common/cmd_parse/hw_parse.h"

template <>
bool NEO::HardwareParse::requiresPipelineSelectBeforeMediaState<GenGfxFamily>() {
    return false;
}

#include "shared/test/common/cmd_parse/cmd_parse_pvc_and_later.inl"

template <>
void HardwareParse::findCsrBaseAddress<GenGfxFamily>() {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GenGfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<STATE_CONTEXT_DATA_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    if (itorGpgpuCsrBaseAddress != cmdList.end()) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);
