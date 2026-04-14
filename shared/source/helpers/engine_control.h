/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace aub_stream {
enum EngineType : uint32_t;
}

namespace NEO {
enum class EngineUsage : uint32_t;
class OsContext;
class CommandStreamReceiver;

struct EngineControl {
    EngineControl() = default;
    EngineControl(CommandStreamReceiver *commandStreamReceiver, OsContext *osContext)
        : commandStreamReceiver(commandStreamReceiver),
          osContext(osContext) {};

    CommandStreamReceiver *commandStreamReceiver = nullptr;
    OsContext *osContext = nullptr;

    const aub_stream::EngineType &getEngineType() const;
    EngineUsage getEngineUsage() const;
};
} // namespace NEO
