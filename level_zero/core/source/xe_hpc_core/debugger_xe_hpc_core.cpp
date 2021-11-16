/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"

namespace NEO {
struct XE_HPC_COREFamily;
using GfxFamily = XE_HPC_COREFamily;

} // namespace NEO

namespace L0 {
template class DebuggerL0Hw<NEO::GfxFamily>;
DebuggerL0PopulateFactory<IGFX_XE_HPC_CORE, NEO::GfxFamily> debuggerXeHpcCore;
} // namespace L0