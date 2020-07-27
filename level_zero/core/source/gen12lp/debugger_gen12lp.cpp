/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"

namespace NEO {
struct TGLLPFamily;
using GfxFamily = TGLLPFamily;
} // namespace NEO

namespace L0 {
template class DebuggerL0Hw<NEO::GfxFamily>;
static DebuggerL0PopulateFactory<IGFX_GEN12LP_CORE, NEO::GfxFamily> debuggerGen12lp;
} // namespace L0