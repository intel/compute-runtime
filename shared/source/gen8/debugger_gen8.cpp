/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.inl"
#include "shared/source/debugger/debugger_l0_base.inl"

namespace NEO {

struct BDWFamily;
using GfxFamily = BDWFamily;

template class DebuggerL0Hw<NEO::GfxFamily>;
} // namespace NEO