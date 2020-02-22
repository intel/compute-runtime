/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
MI_ARB_CHECK *genCmdCast<MI_ARB_CHECK *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_ARB_CHECK *>(buffer);

    return MI_ARB_CHECK::MI_INSTRUCTION_TYPE_MI_INSTRUCTION == pCmd->TheStructure.Common.MiInstructionType &&
                   MI_ARB_CHECK::MI_INSTRUCTION_OPCODE_MI_ARB_CHECK == pCmd->TheStructure.Common.MiInstructionOpcode
               ? pCmd
               : nullptr;
}
