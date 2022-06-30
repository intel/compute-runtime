/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_tgllp_and_later.inl"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {

using Family = NEO::TGLLPFamily;

static auto coreFamily = IGFX_GEN12LP_CORE;

template <>
void populateFactoryTable<DebuggerL0Hw<Family>>() {
    extern DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE];
    debuggerL0Factory[coreFamily] = DebuggerL0Hw<Family>::allocate;
}
template class DebuggerL0Hw<Family>;
} // namespace NEO