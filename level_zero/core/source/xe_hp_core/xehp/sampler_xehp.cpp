/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hp_core/sampler_xe_hp_core.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_XE_HP_SDV> : public SamplerCoreFamily<IGFX_XE_HP_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_XE_HP_SDV, SamplerProductFamily<IGFX_XE_HP_SDV>> populateXEHP;

} // namespace L0
