/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/gen_common/gen_cmd_parse.h"

// clang-format off
using namespace NEO;
using MI_ARB_CHECK                    = GenStruct::MI_ARB_CHECK;
using MI_ATOMIC                       = GenStruct::MI_ATOMIC;
using MI_BATCH_BUFFER_END             = GenStruct::MI_BATCH_BUFFER_END;
using MI_BATCH_BUFFER_START           = GenStruct::MI_BATCH_BUFFER_START;
using MI_LOAD_REGISTER_IMM            = GenStruct::MI_LOAD_REGISTER_IMM;
using MI_LOAD_REGISTER_MEM            = GenStruct::MI_LOAD_REGISTER_MEM;
using MI_STORE_REGISTER_MEM           = GenStruct::MI_STORE_REGISTER_MEM;
using MI_NOOP                         = GenStruct::MI_NOOP;
using PIPE_CONTROL                    = GenStruct::PIPE_CONTROL;
using PIPELINE_SELECT                 = GenStruct::PIPELINE_SELECT;
using STATE_BASE_ADDRESS              = GenStruct::STATE_BASE_ADDRESS;
using MI_REPORT_PERF_COUNT            = GenStruct::MI_REPORT_PERF_COUNT;
using MI_MATH                         = GenStruct::MI_MATH;
using MI_LOAD_REGISTER_REG            = GenStruct::MI_LOAD_REGISTER_REG;
using MI_SEMAPHORE_WAIT               = GenStruct::MI_SEMAPHORE_WAIT;
using MI_STORE_DATA_IMM               = GenStruct::MI_STORE_DATA_IMM;
using MI_FLUSH_DW                     = GenStruct::MI_FLUSH_DW;
using XY_COPY_BLT                     = GenGfxFamily::XY_COPY_BLT;
// clang-format on

template <>
STATE_BASE_ADDRESS *genCmdCast<STATE_BASE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(buffer);

    return STATE_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   STATE_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   STATE_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   STATE_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_STATE_BASE_ADDRESS == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
PIPE_CONTROL *genCmdCast<PIPE_CONTROL *>(void *buffer) {
    auto pCmd = reinterpret_cast<PIPE_CONTROL *>(buffer);

    return PIPE_CONTROL::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   PIPE_CONTROL::COMMAND_SUBTYPE_GFXPIPE_3D == pCmd->TheStructure.Common.CommandSubtype &&
                   PIPE_CONTROL::_3D_COMMAND_OPCODE_PIPE_CONTROL == pCmd->TheStructure.Common._3DCommandOpcode &&
                   PIPE_CONTROL::_3D_COMMAND_SUB_OPCODE_PIPE_CONTROL == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
PIPELINE_SELECT *genCmdCast<PIPELINE_SELECT *>(void *buffer) {
    auto pCmd = reinterpret_cast<PIPELINE_SELECT *>(buffer);

    return PIPELINE_SELECT::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   PIPELINE_SELECT::COMMAND_SUBTYPE_GFXPIPE_SINGLE_DW == pCmd->TheStructure.Common.CommandSubtype &&
                   PIPELINE_SELECT::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   PIPELINE_SELECT::_3D_COMMAND_SUB_OPCODE_PIPELINE_SELECT == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_LOAD_REGISTER_IMM *genCmdCast<MI_LOAD_REGISTER_IMM *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(buffer);

    return MI_LOAD_REGISTER_IMM::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_LOAD_REGISTER_IMM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_IMM == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_NOOP *genCmdCast<MI_NOOP *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_NOOP *>(buffer);

    return MI_NOOP::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_NOOP::MI_COMMAND_OPCODE_MI_NOOP == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_ATOMIC *genCmdCast<MI_ATOMIC *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_ATOMIC *>(buffer);

    return MI_ATOMIC::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_ATOMIC::MI_COMMAND_OPCODE_MI_ATOMIC == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_BATCH_BUFFER_END *genCmdCast<MI_BATCH_BUFFER_END *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_BATCH_BUFFER_END *>(buffer);

    return MI_BATCH_BUFFER_END::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_BATCH_BUFFER_END::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_END == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_BATCH_BUFFER_START *genCmdCast<MI_BATCH_BUFFER_START *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(buffer);

    return MI_BATCH_BUFFER_START::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_BATCH_BUFFER_START::MI_COMMAND_OPCODE_MI_BATCH_BUFFER_START == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_LOAD_REGISTER_MEM *genCmdCast<MI_LOAD_REGISTER_MEM *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(buffer);

    return MI_LOAD_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_LOAD_REGISTER_MEM::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_MEM == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_STORE_REGISTER_MEM *genCmdCast<MI_STORE_REGISTER_MEM *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(buffer);

    return MI_STORE_REGISTER_MEM::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_STORE_REGISTER_MEM::MI_COMMAND_OPCODE_MI_STORE_REGISTER_MEM == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_REPORT_PERF_COUNT *genCmdCast<MI_REPORT_PERF_COUNT *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_REPORT_PERF_COUNT *>(buffer);

    return MI_REPORT_PERF_COUNT::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_REPORT_PERF_COUNT::MI_COMMAND_OPCODE_MI_REPORT_PERF_COUNT == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_MATH *genCmdCast<MI_MATH *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_MATH *>(buffer);

    return MI_MATH::COMMAND_TYPE_MI_COMMAND == pCmd->DW0.BitField.InstructionType &&
                   MI_MATH::MI_COMMAND_OPCODE_MI_MATH == pCmd->DW0.BitField.InstructionOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_LOAD_REGISTER_REG *genCmdCast<MI_LOAD_REGISTER_REG *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_REG *>(buffer);

    return MI_LOAD_REGISTER_REG::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_LOAD_REGISTER_REG::MI_COMMAND_OPCODE_MI_LOAD_REGISTER_REG == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_SEMAPHORE_WAIT *genCmdCast<MI_SEMAPHORE_WAIT *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

    return MI_SEMAPHORE_WAIT::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_SEMAPHORE_WAIT::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_STORE_DATA_IMM *genCmdCast<MI_STORE_DATA_IMM *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(buffer);

    return MI_STORE_DATA_IMM::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_STORE_DATA_IMM::MI_COMMAND_OPCODE_MI_STORE_DATA_IMM == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_FLUSH_DW *genCmdCast<MI_FLUSH_DW *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    return MI_FLUSH_DW::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_FLUSH_DW::MI_COMMAND_OPCODE_MI_FLUSH_DW == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <>
XY_COPY_BLT *genCmdCast<XY_COPY_BLT *>(void *buffer) {
    auto pCmd = reinterpret_cast<XY_COPY_BLT *>(buffer);

    return XY_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
                   XY_COPY_BLT::CLIENT_2D_PROCESSOR == pCmd->TheStructure.Common.Client
               ? pCmd
               : nullptr;
}

template <class T>
size_t CmdParse<T>::getCommandLength(void *cmd) {
    {
        auto pCmd = genCmdCast<STATE_BASE_ADDRESS *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<PIPE_CONTROL *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_ARB_CHECK *>(cmd);
        if (pCmd)
            return SIZE32(*pCmd);
    }
    {
        auto pCmd = genCmdCast<MI_ATOMIC *>(cmd);
        if (pCmd)
            return sizeof(MI_ATOMIC) / sizeof(uint32_t);
    }
    {
        auto pCmd = genCmdCast<MI_BATCH_BUFFER_END *>(cmd);
        if (pCmd)
            return SIZE32(*pCmd);
    }
    {
        auto pCmd = genCmdCast<MI_BATCH_BUFFER_START *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_MEM *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_NOOP *>(cmd);
        if (pCmd)
            return SIZE32(*pCmd);
    }
    {
        auto pCmd = genCmdCast<PIPELINE_SELECT *>(cmd);
        if (pCmd)
            return SIZE32(*pCmd);
    }
    {
        auto pCmd = genCmdCast<MI_REPORT_PERF_COUNT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_MATH *>(cmd);
        if (pCmd)
            return pCmd->DW0.BitField.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_STORE_DATA_IMM *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 3;
    }
    {
        auto pCmd = genCmdCast<MI_FLUSH_DW *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<XY_COPY_BLT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    return getCommandLengthHwSpecific(cmd);
}

template <class T>
const char *CmdParse<T>::getCommandName(void *cmd) {
#define RETURN_NAME_IF(CMD_NAME)                \
    if (nullptr != genCmdCast<CMD_NAME *>(cmd)) \
        return #CMD_NAME;

    RETURN_NAME_IF(STATE_BASE_ADDRESS);
    RETURN_NAME_IF(PIPE_CONTROL);
    RETURN_NAME_IF(MI_ARB_CHECK);
    RETURN_NAME_IF(MI_ATOMIC);
    RETURN_NAME_IF(MI_BATCH_BUFFER_END);
    RETURN_NAME_IF(MI_BATCH_BUFFER_START);
    RETURN_NAME_IF(MI_LOAD_REGISTER_IMM);
    RETURN_NAME_IF(MI_LOAD_REGISTER_MEM);
    RETURN_NAME_IF(MI_STORE_REGISTER_MEM);
    RETURN_NAME_IF(MI_NOOP);
    RETURN_NAME_IF(PIPELINE_SELECT);
    RETURN_NAME_IF(MI_REPORT_PERF_COUNT);
    RETURN_NAME_IF(MI_MATH);
    RETURN_NAME_IF(MI_LOAD_REGISTER_REG);
    RETURN_NAME_IF(MI_SEMAPHORE_WAIT);
    RETURN_NAME_IF(MI_STORE_DATA_IMM);
    RETURN_NAME_IF(MI_FLUSH_DW);
    RETURN_NAME_IF(XY_COPY_BLT);

#undef RETURN_NAME_IF

    return getCommandNameHwSpecific(cmd);
}
