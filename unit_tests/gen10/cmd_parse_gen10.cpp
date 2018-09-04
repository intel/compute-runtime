/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/hw_parse.inl"
#include "gtest/gtest.h"

// clang-format off
using namespace OCLRT;
using GPGPU_WALKER                    = GEN10::GPGPU_WALKER;
using MEDIA_INTERFACE_DESCRIPTOR_LOAD = GEN10::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
using MEDIA_STATE_FLUSH               = GEN10::MEDIA_STATE_FLUSH;
using MEDIA_VFE_STATE                 = GEN10::MEDIA_VFE_STATE;
using MI_ARB_CHECK                    = GEN10::MI_ARB_CHECK;
using MI_ATOMIC                       = GEN10::MI_ATOMIC;
using MI_BATCH_BUFFER_END             = GEN10::MI_BATCH_BUFFER_END;
using MI_BATCH_BUFFER_START           = GEN10::MI_BATCH_BUFFER_START;
using MI_LOAD_REGISTER_IMM            = GEN10::MI_LOAD_REGISTER_IMM;
using MI_LOAD_REGISTER_MEM            = GEN10::MI_LOAD_REGISTER_MEM;
using MI_STORE_REGISTER_MEM           = GEN10::MI_STORE_REGISTER_MEM;
using MI_NOOP                         = GEN10::MI_NOOP;
using PIPE_CONTROL                    = GEN10::PIPE_CONTROL;
using PIPELINE_SELECT                 = GEN10::PIPELINE_SELECT;
using STATE_BASE_ADDRESS              = GEN10::STATE_BASE_ADDRESS;
using MI_REPORT_PERF_COUNT            = GEN10::MI_REPORT_PERF_COUNT;
using MI_LOAD_REGISTER_REG            = GEN10::MI_LOAD_REGISTER_REG;
using MI_MATH                         = GEN10::MI_MATH;
using GPGPU_CSR_BASE_ADDRESS          = GEN10::GPGPU_CSR_BASE_ADDRESS;
using STATE_SIP                       = GEN10::STATE_SIP;
using MI_SEMAPHORE_WAIT               = GEN10::MI_SEMAPHORE_WAIT;
using MI_STORE_DATA_IMM               = GEN10::MI_STORE_DATA_IMM;
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
GPGPU_WALKER *genCmdCast<GPGPU_WALKER *>(void *buffer) {
    auto pCmd = reinterpret_cast<GPGPU_WALKER *>(buffer);

    return GPGPU_WALKER::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   GPGPU_WALKER::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   GPGPU_WALKER::MEDIA_COMMAND_OPCODE_GPGPU_WALKER == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   GPGPU_WALKER::SUBOPCODE_GPGPU_WALKER_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_INTERFACE_DESCRIPTOR_LOAD *genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(buffer);

    return MEDIA_INTERFACE_DESCRIPTOR_LOAD::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::MEDIA_COMMAND_OPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_INTERFACE_DESCRIPTOR_LOAD::SUBOPCODE_MEDIA_INTERFACE_DESCRIPTOR_LOAD_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_VFE_STATE *genCmdCast<MEDIA_VFE_STATE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_VFE_STATE *>(buffer);

    return MEDIA_VFE_STATE::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_VFE_STATE::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_VFE_STATE::MEDIA_COMMAND_OPCODE_MEDIA_VFE_STATE == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_VFE_STATE::SUBOPCODE_MEDIA_VFE_STATE_SUBOP == pCmd->TheStructure.Common.Subopcode
               ? pCmd
               : nullptr;
}

template <>
MEDIA_STATE_FLUSH *genCmdCast<MEDIA_STATE_FLUSH *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEDIA_STATE_FLUSH *>(buffer);

    return MEDIA_STATE_FLUSH::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   MEDIA_STATE_FLUSH::PIPELINE_MEDIA == pCmd->TheStructure.Common.Pipeline &&
                   MEDIA_STATE_FLUSH::MEDIA_COMMAND_OPCODE_MEDIA_STATE_FLUSH == pCmd->TheStructure.Common.MediaCommandOpcode &&
                   MEDIA_STATE_FLUSH::SUBOPCODE_MEDIA_STATE_FLUSH_SUBOP == pCmd->TheStructure.Common.Subopcode
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
MI_ARB_CHECK *genCmdCast<MI_ARB_CHECK *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_ARB_CHECK *>(buffer);

    return MI_ARB_CHECK::MI_INSTRUCTION_TYPE_MI_INSTRUCTION == pCmd->TheStructure.Common.MiInstructionType &&
                   MI_ARB_CHECK::MI_INSTRUCTION_OPCODE_MI_ARB_CHECK == pCmd->TheStructure.Common.MiInstructionOpcode
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
GPGPU_CSR_BASE_ADDRESS *genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(buffer);

    return GPGPU_CSR_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   GPGPU_CSR_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_GPGPU_CSR_BASE_ADDRESS == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
STATE_SIP *genCmdCast<STATE_SIP *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_SIP *>(buffer);

    return STATE_SIP::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP == pCmd->TheStructure.Common._3DCommandSubOpcode
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

size_t CnlParse::getCommandLength(void *cmd) {
    {
        auto pCmd = genCmdCast<STATE_BASE_ADDRESS *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<GPGPU_WALKER *>(cmd);
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
            return pCmd->TheStructure.Common.DwordLength + 2;
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
        auto pCmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MEDIA_VFE_STATE *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MEDIA_STATE_FLUSH *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_REPORT_PERF_COUNT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_MATH *>(cmd);
        if (pCmd)
            return pCmd->DW0.BitField.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<STATE_SIP *>(cmd);
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
    return 0;
}

bool CnlParse::parseCommandBuffer(GenCmdList &cmds, void *buffer, size_t length) {
    if (!buffer || length % sizeof(uint32_t)) {
        return false;
    }

    void *bufferEnd = reinterpret_cast<uint8_t *>(buffer) + length;
    while (buffer < bufferEnd) {
        size_t length = getCommandLength(buffer);
        if (!length) {
            return false;
        }

        cmds.push_back(buffer);

        buffer = reinterpret_cast<uint32_t *>(buffer) + length;
    }

    return buffer == bufferEnd;
}

// MIDL should have a MSF between it and a previous walker
template <>
void CnlParse::validateCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
    auto itorCurrent = itorBegin;
    auto itorWalker = itorEnd;

    // Find last GPGPU_WALKER prior to itorCmd
    while (itorCurrent != itorEnd) {
        if (genCmdCast<GPGPU_WALKER *>(*itorCurrent)) {
            itorWalker = itorCurrent;
        }

        ++itorCurrent;
    }

    // If we don't find a GPGPU_WALKER, assume the beginning of a cmd list
    itorWalker = itorWalker == itorEnd
                     ? itorBegin
                     : itorWalker;

    // Look for MEDIA_STATE_FLUSH between last GPGPU_WALKER and MIDL.
    auto itorMSF = itorEnd;

    itorCurrent = itorWalker;
    ++itorCurrent;
    while (itorCurrent != itorEnd) {
        if (genCmdCast<MEDIA_STATE_FLUSH *>(*itorCurrent)) {
            itorMSF = itorCurrent;
            break;
        }
        ++itorCurrent;
    }

    ASSERT_FALSE(itorMSF == itorEnd) << "A MEDIA_STATE_FLUSH is required before a MEDIA_INTERFACE_DESCRIPTOR_LOAD.";
}

template <>
void CnlParse::validateCommand<STATE_BASE_ADDRESS *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
}

// MVFES should have a stalling PC between it and a previous walker
template <>
void CnlParse::validateCommand<MEDIA_VFE_STATE *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
    auto itorCurrent = itorBegin;
    auto itorWalker = itorEnd;

    // Find last GPGPU_WALKER prior to itorCmd
    while (itorCurrent != itorEnd) {
        if (genCmdCast<GPGPU_WALKER *>(*itorCurrent)) {
            itorWalker = itorCurrent;
        }

        ++itorCurrent;
    }

    // If we don't find a GPGPU_WALKER, assume the beginning of a cmd list
    itorWalker = itorWalker == itorEnd
                     ? itorBegin
                     : itorWalker;

    // Look for PIPE_CONTROL between last GPGPU_WALKER and MVFES.
    itorCurrent = itorWalker;
    ++itorCurrent;
    while (itorCurrent != itorEnd) {
        if (genCmdCast<PIPE_CONTROL *>(*itorCurrent)) {
            auto pPC = genCmdCast<PIPE_CONTROL *>(*itorCurrent);
            if (pPC->getCommandStreamerStallEnable()) {
                return;
            }
        }
        ++itorCurrent;
    }

    ASSERT_TRUE(false) << "A PIPE_CONTROL w/ CS stall is required before a MEDIA_VFE_STATE.";
}

namespace OCLRT {
template void HardwareParse::findHardwareCommands<CNLFamily>();
template const void *HardwareParse::getStatelessArgumentPointer<CNLFamily>(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh);
} // namespace OCLRT
