/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"

namespace NEO {
struct XeHpFamily;
using GfxFamily = XeHpFamily;

} // namespace NEO

namespace L0 {
template class DebuggerL0Hw<NEO::GfxFamily>;
DebuggerL0PopulateFactory<IGFX_XE_HP_CORE, NEO::GfxFamily> debuggerXE_HP_CORE;
} // namespace L0
