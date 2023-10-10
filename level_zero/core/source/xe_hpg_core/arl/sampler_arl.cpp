/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/xe_hpg_core/sampler_xe_hpg_core.inl"

namespace L0 {
template struct SamplerCoreFamily<IGFX_XE_HPG_CORE>;
template <>
struct SamplerProductFamily<IGFX_ARROWLAKE> : public SamplerCoreFamily<IGFX_XE_HPG_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;

    using SAMPLER_STATE = typename NEO::XeHpgCoreFamily::SAMPLER_STATE;

    ze_result_t initialize(Device *device, const ze_sampler_desc_t *desc) override {
        return SamplerCoreFamily<IGFX_XE_HPG_CORE>::initialize(device, desc);
    };
};

static SamplerPopulateFactory<IGFX_ARROWLAKE, SamplerProductFamily<IGFX_ARROWLAKE>> populateARL;

} // namespace L0
