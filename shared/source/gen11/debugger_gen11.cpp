/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_base.inl"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {
struct ICLFamily;
using GfxFamily = ICLFamily;
static auto coreFamily = IGFX_GEN11_CORE;

template <>
void populateFactoryTable<DebuggerL0Hw<GfxFamily>>() {
    extern DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE];
    debuggerL0Factory[coreFamily] = DebuggerL0Hw<GfxFamily>::allocate;
}
template class DebuggerL0Hw<GfxFamily>;
} // namespace NEO
