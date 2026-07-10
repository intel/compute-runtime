/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using MI_MEM_FENCE = GenStruct::MI_MEM_FENCE;

template <>
MI_MEM_FENCE *genCmdCast<MI_MEM_FENCE *>(void *buffer) {
    return matchCommandHeader<MI_MEM_FENCE>(buffer, [](const MI_MEM_FENCE &header) {
        return 0x0 == header.TheStructure.Common.MiCommandSubOpcode &&
               0x9 == header.TheStructure.Common.MiCommandOpcode &&
               0x0 == header.TheStructure.Common.CommandType;
    });
}
