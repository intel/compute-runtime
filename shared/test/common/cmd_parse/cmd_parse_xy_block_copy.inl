/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using XY_BLOCK_COPY_BLT = GenStruct::XY_BLOCK_COPY_BLT;

template <>
XY_BLOCK_COPY_BLT *genCmdCast<XY_BLOCK_COPY_BLT *>(void *buffer) {
    auto pCmd = reinterpret_cast<XY_BLOCK_COPY_BLT *>(buffer);

    return XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
                   XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR == pCmd->TheStructure.Common.Client
               ? pCmd
               : nullptr;
}
