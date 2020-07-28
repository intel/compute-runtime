/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "engine_node.h"

namespace NEO {
class CommandStreamReceiver;
class OsContext;

struct EngineControl {
    EngineControl() = default;
    EngineControl(CommandStreamReceiver *commandStreamReceiver, OsContext *osContext)
        : commandStreamReceiver(commandStreamReceiver),
          osContext(osContext),
          engineType(aub_stream::EngineType::ENGINE_RCS){};

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    OsContext *osContext = nullptr;
    aub_stream::EngineType engineType;
};
} // namespace NEO
