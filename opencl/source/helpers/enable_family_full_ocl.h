/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/populate_factory.h"

namespace NEO {

template <typename GfxFamily>
class BufferHw;

template <typename GfxFamily>
class ClGfxCoreHelperHw;

template <typename GfxFamily>
class CommandQueueHw;

template <typename GfxFamily>
class ImageHw;

template <typename GfxFamily>
struct SamplerHw;

template <typename GfxFamily>
struct EnableOCL {
    EnableOCL() {
        populateFactoryTable<BufferHw<GfxFamily>>();
        populateFactoryTable<ClGfxCoreHelperHw<GfxFamily>>();
        populateFactoryTable<CommandQueueHw<GfxFamily>>();
        populateFactoryTable<ImageHw<GfxFamily>>();
        populateFactoryTable<SamplerHw<GfxFamily>>();
    }
};

} // namespace NEO
