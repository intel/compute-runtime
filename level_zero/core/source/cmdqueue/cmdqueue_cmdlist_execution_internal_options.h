/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <mutex>

namespace NEO {
class LinearStream;
} // namespace NEO

namespace L0 {

struct CommandListExecutionInternalOptions {
    uint64_t patchPreambleRequiredCounter = 0;

    NEO::LinearStream *parentImmediateCommandlistLinearStream = nullptr;
    std::unique_lock<std::mutex> *outerLockForIndirect = nullptr;

    bool performMigration = false;
};

} // namespace L0
