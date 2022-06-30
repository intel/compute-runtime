/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

namespace NEO {
struct CopyEngineState {
    aub_stream::EngineType engineType = aub_stream::EngineType::NUM_ENGINES;
    uint32_t taskCount = 0;

    bool isValid() const {
        return engineType != aub_stream::EngineType::NUM_ENGINES;
    }
};
} // namespace NEO
