/*
 * Copyright (C) 2018-2022 Intel Corporation
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

    const aub_stream::EngineType &getEngineType() const { return osContext->getEngineType(); }
    EngineUsage getEngineUsage() const { return osContext->getEngineUsage(); }
};
} // namespace NEO
