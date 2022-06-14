/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_base.inl"

namespace NEO {
struct ICLFamily;
using GfxFamily = ICLFamily;

template class DebuggerL0Hw<NEO::GfxFamily>;
static DebuggerL0PopulateFactory<IGFX_GEN11_CORE, NEO::GfxFamily> debuggerGen11;
} // namespace NEO