/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_tgllp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {

using GfxFamily = NEO::Xe2HpgCoreFamily;
static auto coreFamily = IGFX_XE2_HPG_CORE;

template <>
void populateFactoryTable<DebuggerL0Hw<GfxFamily>>() {
    extern DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE];
    debuggerL0Factory[coreFamily] = DebuggerL0Hw<GfxFamily>::allocate;
}
template class DebuggerL0Hw<GfxFamily>;

} // namespace NEO
