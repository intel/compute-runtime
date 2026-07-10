/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using STATE_PREFETCH = GenStruct::STATE_PREFETCH;

template <>
STATE_PREFETCH *genCmdCast<STATE_PREFETCH *>(void *buffer) {
    return matchCommandHeader<STATE_PREFETCH>(buffer, [](const STATE_PREFETCH &header) {
        return 0x2 == header.TheStructure.Common.DwordLength &&
               0x3 == header.TheStructure.Common._3DCommandSubOpcode &&
               0x0 == header.TheStructure.Common._3DCommandOpcode &&
               0x0 == header.TheStructure.Common.CommandSubtype &&
               0x3 == header.TheStructure.Common.CommandType;
    });
}
