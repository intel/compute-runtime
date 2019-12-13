/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "runtime/sampler/sampler.h"
#include "runtime/sampler/sampler.inl"

namespace NEO {

using SAMPLER_STATE = typename Family::SAMPLER_STATE;

template <>
void SamplerHw<Family>::appendSamplerStateParams(SAMPLER_STATE *state) {
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        state->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

#include "runtime/sampler/sampler_factory_init.inl"
} // namespace NEO
