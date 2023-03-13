/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"

#include "aubstream/engine_node.h"

namespace NEO {
struct CopyEngineState {
    aub_stream::EngineType engineType = aub_stream::EngineType::NUM_ENGINES;
    TaskCountType taskCount = 0;
    bool csrClientRegistered = false;

    bool isValid() const {
        return engineType != aub_stream::EngineType::NUM_ENGINES;
    }
};
} // namespace NEO
