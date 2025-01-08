/*
 * Copyright (C) 2024-2025 Intel Corporation
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

    return (MEM_SET::DWORD_LENGTH::DWORD_LENGTH_EXCLUDES_DWORD_0_1 == pCmd->TheStructure.Common.DwordLength &&
            MEM_SET::INSTRUCTION_TARGETOPCODE::INSTRUCTION_TARGETOPCODE_MEM_SET == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
            MEM_SET::CLIENT::CLIENT_2D_PROCESSOR == pCmd->TheStructure.Common.Client)
               ? pCmd
               : nullptr;
}
