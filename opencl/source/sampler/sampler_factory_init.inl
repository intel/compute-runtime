/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Sampler factory table initialization.
// Family, gfxCore came from outside, do not set them here unless you
// really know what you are doing

template struct SamplerHw<Family>;
template <>
void populateFactoryTable<SamplerHw<Family>>() {
    extern SamplerCreateFunc samplerFactory[IGFX_MAX_CORE];
    samplerFactory[gfxCore] = SamplerHw<Family>::create;
}
