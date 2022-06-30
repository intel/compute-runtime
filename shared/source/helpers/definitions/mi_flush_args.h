/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
struct MiFlushArgs {
    bool timeStampOperation = false;
    bool commandWithPostSync = false;
    bool notifyEnable = false;
    bool tlbFlush = false;

    MiFlushArgs() = default;
};
} // namespace NEO
