/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using MEM_SET = GenStruct::MEM_SET;

template <>
MEM_SET *genCmdCast<MEM_SET *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEM_SET *>(buffer);

    return (0x5 == pCmd->TheStructure.Common.DwordLength &&
            0x5B == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
            0x2 == pCmd->TheStructure.Common.Client)
               ? pCmd
               : nullptr;
}
