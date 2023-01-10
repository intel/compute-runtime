/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/engine_node_helper.h"

namespace aub_stream {
enum EngineType : uint32_t;
}

namespace NEO {
class OsContext;
class CommandStreamReceiver;

struct EngineControl {
    EngineControl() = default;
    EngineControl(CommandStreamReceiver *commandStreamReceiver, OsContext *osContext)
        : commandStreamReceiver(commandStreamReceiver),
          osContext(osContext){};

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    OsContext *osContext = nullptr;

    const aub_stream::EngineType &getEngineType() const;
    EngineUsage getEngineUsage() const;
};
} // namespace NEO
