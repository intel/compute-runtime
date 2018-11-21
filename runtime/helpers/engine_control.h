/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {
class CommandStreamReceiver;
class OsContext;

struct EngineControl {
    EngineControl() = default;
    EngineControl(CommandStreamReceiver *commandStreamReceiver, OsContext *osContext)
        : commandStreamReceiver(commandStreamReceiver), osContext(osContext){};

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    OsContext *osContext = nullptr;
};
} // namespace OCLRT
