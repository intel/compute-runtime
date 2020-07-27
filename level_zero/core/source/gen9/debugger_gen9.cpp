/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"

namespace NEO {
struct SKLFamily;
using GfxFamily = SKLFamily;
} // namespace NEO

namespace L0 {
template class DebuggerL0Hw<NEO::GfxFamily>;
static DebuggerL0PopulateFactory<IGFX_GEN9_CORE, NEO::GfxFamily> debuggerGen9;
} // namespace L0