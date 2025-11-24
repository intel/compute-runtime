/*
 * Copyright (C) 2018-2025 Intel Corporation
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
    extern SamplerCreateFunc samplerFactory[NEO::maxCoreEnumValue];
    samplerFactory[gfxCore] = SamplerHw<Family>::create;
}
