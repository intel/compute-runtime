/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/sampler_imp.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct SamplerCoreFamily : public SamplerImp {
  public:
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    using BaseClass = SamplerImp;

    ze_result_t initialize(Device *device, const ze_sampler_desc_t *desc) override;
    void copySamplerStateToDSH(void *dynamicStateHeap, const uint32_t dynamicStateHeapSize,
                               const uint32_t offset) override;

    static constexpr float getGenSamplerMaxLod() {
        return 14.0f;
    }

  protected:
    SAMPLER_STATE samplerState;
    float lodMin = 1.0f;
    float lodMax = 1.0f;
};

template <uint32_t gfxProductFamily>
struct SamplerProductFamily;

} // namespace L0
