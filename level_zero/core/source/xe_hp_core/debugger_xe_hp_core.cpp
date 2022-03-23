/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.inl"
#include "level_zero/core/source/debugger/debugger_l0_tgllp_and_later.inl"
namespace L0 {

using Family = NEO::XeHpFamily;

DebuggerL0PopulateFactory<IGFX_XE_HP_CORE, Family> debuggerXE_HP_CORE;

template class DebuggerL0Hw<Family>;

} // namespace L0
