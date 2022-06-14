/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_tgllp_and_later.inl"
namespace NEO {

using Family = NEO::TGLLPFamily;

template class DebuggerL0Hw<Family>;
static DebuggerL0PopulateFactory<IGFX_GEN12LP_CORE, Family> debuggerGen12lp;
} // namespace NEO