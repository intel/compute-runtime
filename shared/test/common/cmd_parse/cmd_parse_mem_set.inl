/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using MEM_SET = GenStruct::MEM_SET;

template <>
MEM_SET *genCmdCast<MEM_SET *>(void *buffer) {
    return matchCommandHeader<MEM_SET>(buffer, [](const MEM_SET &header) {
        return MEM_SET::sInit().TheStructure.Common.DwordLength == header.TheStructure.Common.DwordLength &&
               MEM_SET::sInit().getInstructionTargetOpcode() == header.getInstructionTargetOpcode() &&
               MEM_SET::CLIENT::CLIENT_2D_PROCESSOR == header.TheStructure.Common.Client;
    });
}
