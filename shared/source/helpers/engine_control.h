/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"

#include "engine_node.h"

namespace NEO {
class CommandStreamReceiver;

struct EngineControl {
    EngineControl() = default;
    EngineControl(CommandStreamReceiver *commandStreamReceiver, OsContext *osContext)
        : commandStreamReceiver(commandStreamReceiver),
          osContext(osContext){};

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    OsContext *osContext = nullptr;

    aub_stream::EngineType &getEngineType() { return osContext->getEngineType(); }
};
} // namespace NEO
