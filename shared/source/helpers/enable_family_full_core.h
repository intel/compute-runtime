/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/populate_factory.h"

namespace NEO {

template <typename GfxFamily>
class AUBCommandStreamReceiverHw;
template <typename GfxFamily>
class CommandStreamReceiverHw;
template <typename GfxFamily>
class TbxCommandStreamReceiverHw;
template <typename GfxFamily>
class DebuggerL0Hw;
template <typename GfxFamily>
class GfxCoreHelperHw;
template <typename GfxFamily>
struct GmmCallbacks;

template <typename GfxFamily>
struct EnableCore {
    EnableCore() {
        populateFactoryTable<GfxCoreHelperHw<GfxFamily>>();
        populateFactoryTable<AUBCommandStreamReceiverHw<GfxFamily>>();
        populateFactoryTable<CommandStreamReceiverHw<GfxFamily>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<GfxFamily>>();
        populateFactoryTable<DebuggerL0Hw<GfxFamily>>();
        populateFactoryTable<GmmCallbacks<GfxFamily>>();
    }
};

} // namespace NEO
