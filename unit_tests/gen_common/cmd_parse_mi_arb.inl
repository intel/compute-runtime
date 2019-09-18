/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
MI_ARB_CHECK *genCmdCast<MI_ARB_CHECK *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_ARB_CHECK *>(buffer);

    return MI_ARB_CHECK::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_ARB_CHECK::MI_COMMAND_OPCODE_MI_ARB_CHECK == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}
