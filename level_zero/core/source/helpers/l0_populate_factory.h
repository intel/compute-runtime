/*
 * Copyright (C) 2020-2025 Intel Corporation
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

} // namespace L0
