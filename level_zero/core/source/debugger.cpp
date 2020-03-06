/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

std::unique_ptr<NEO::Debugger> NEO::Debugger::create(HardwareInfo *hwInfo) {
    return std::make_unique<L0::DebuggerL0>();
}