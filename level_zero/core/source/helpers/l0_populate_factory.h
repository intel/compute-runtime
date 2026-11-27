/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {

template <typename GfxFamily>
class L0GfxCoreHelperHw;

// Method called by global factory enabler
template <typename Type>
void populateFactoryTable();

template <typename GfxFamily>
struct EnableL0 {
    EnableL0() {
        populateFactoryTable<L0GfxCoreHelperHw<GfxFamily>>();
    }
};

} // namespace L0
