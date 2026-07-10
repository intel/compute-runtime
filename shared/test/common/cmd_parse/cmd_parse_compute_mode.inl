/*
 * Copyright (C) 2019-2026 Intel Corporation
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
    return matchCommandHeader<STATE_COMPUTE_MODE>(buffer, [](const STATE_COMPUTE_MODE &header) {
        return STATE_COMPUTE_MODE::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               STATE_COMPUTE_MODE::COMMAND_SUBTYPE_GFXPIPE_COMMON == header.TheStructure.Common.CommandSubtype &&
               STATE_COMPUTE_MODE::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
               STATE_COMPUTE_MODE::_3D_COMMAND_SUB_OPCODE_STATE_COMPUTE_MODE == header.TheStructure.Common._3DCommandSubOpcode;
    });
}
