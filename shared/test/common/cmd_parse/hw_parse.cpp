/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"

#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

LinearStream &HardwareParse::getCsrCommandStream(CommandStreamReceiver &commandStreamReceiver) {
    return commandStreamReceiver.getCS();
}

IndirectHeap &HardwareParse::getCsrDynamicStateHeap(CommandStreamReceiver &commandStreamReceiver) {
    return commandStreamReceiver.getIndirectHeap(IndirectHeap::Type::dynamicState, 0);
}

} // namespace NEO
