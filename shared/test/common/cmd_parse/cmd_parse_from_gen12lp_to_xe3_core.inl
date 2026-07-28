/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

template <>
GenGfxFamily::MI_SEMAPHORE_WAIT *genCmdCast<GenGfxFamily::MI_SEMAPHORE_WAIT *>(void *buffer) {
    using MI_SEMAPHORE_WAIT = GenGfxFamily::MI_SEMAPHORE_WAIT;
    return matchCommandHeader<MI_SEMAPHORE_WAIT>(buffer, [](const MI_SEMAPHORE_WAIT &header) {
        return MI_SEMAPHORE_WAIT::COMMAND_TYPE_MI_COMMAND == header.TheStructure.Common.CommandType &&
               MI_SEMAPHORE_WAIT::MI_COMMAND_OPCODE_MI_SEMAPHORE_WAIT == header.TheStructure.Common.MiCommandOpcode;
    });
}
