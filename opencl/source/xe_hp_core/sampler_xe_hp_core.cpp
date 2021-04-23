/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"
using Family = NEO::XeHpFamily;
constexpr static auto gfxCore = IGFX_XE_HP_CORE;
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sampler/sampler.inl"
namespace NEO {

using SAMPLER_STATE = typename Family::SAMPLER_STATE;

template <>
void SamplerHw<Family>::appendSamplerStateParams(SAMPLER_STATE *state, const HardwareInfo &hwInfo) {
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        state->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

#include "opencl/source/sampler/sampler_factory_init.inl"
} // namespace NEO
