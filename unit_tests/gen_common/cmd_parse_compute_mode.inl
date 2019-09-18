/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using STATE_COMPUTE_MODE              = GenStruct::STATE_COMPUTE_MODE;
// clang-format on

template <>
STATE_COMPUTE_MODE *genCmdCast<STATE_COMPUTE_MODE *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(buffer);

    return STATE_COMPUTE_MODE::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   STATE_COMPUTE_MODE::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   STATE_COMPUTE_MODE::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   STATE_COMPUTE_MODE::_3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}
