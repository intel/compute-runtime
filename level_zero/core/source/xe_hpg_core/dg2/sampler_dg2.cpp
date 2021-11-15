/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/xe_hpg_core/sampler_xe_hpg_core.inl"

namespace L0 {
template struct SamplerCoreFamily<IGFX_XE_HPG_CORE>;
template <>
struct SamplerProductFamily<IGFX_DG2> : public SamplerCoreFamily<IGFX_XE_HPG_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
    void appendSamplerStateParams(SAMPLER_STATE *state) override {
        if (NEO::DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
            state->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
        }
    }
    ze_result_t initialize(Device *device, const ze_sampler_desc_t *desc) override {
        return SamplerCoreFamily<IGFX_XE_HPG_CORE>::initialize(device, desc);
    };
};

static SamplerPopulateFactory<IGFX_DG2, SamplerProductFamily<IGFX_DG2>> populateDG2;

} // namespace L0
