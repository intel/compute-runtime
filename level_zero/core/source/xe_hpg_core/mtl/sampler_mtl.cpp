/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hpg_core/sampler_xe_hpg_core.inl"

namespace L0 {
template struct SamplerCoreFamily<IGFX_XE_HPG_CORE>;
template <>
struct SamplerProductFamily<IGFX_METEORLAKE> : public SamplerCoreFamily<IGFX_XE_HPG_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;

    using SAMPLER_STATE = typename NEO::XeHpgCoreFamily::SAMPLER_STATE;

    ze_result_t initialize(Device *device, const ze_sampler_desc_t *desc) override {
        return SamplerCoreFamily<IGFX_XE_HPG_CORE>::initialize(device, desc);
    };
};

static SamplerPopulateFactory<IGFX_METEORLAKE, SamplerProductFamily<IGFX_METEORLAKE>> populateMTL;

} // namespace L0
