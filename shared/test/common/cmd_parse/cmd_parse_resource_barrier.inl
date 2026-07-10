/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using RESOURCE_BARRIER = GenStruct::RESOURCE_BARRIER;

template <>
RESOURCE_BARRIER *genCmdCast<RESOURCE_BARRIER *>(void *buffer) {
    return matchCommandHeader<RESOURCE_BARRIER>(buffer, [](const RESOURCE_BARRIER &header) {
        return 0x3 == header.TheStructure.Common.DwordLength &&
               0x3 == header.TheStructure.Common.Opcode &&
               0x5 == header.TheStructure.Common.CommandType;
    });
}
