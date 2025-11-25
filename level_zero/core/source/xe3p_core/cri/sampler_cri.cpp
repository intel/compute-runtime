/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<NEO::criProductEnumValue> : public SamplerCoreFamily<NEO::xe3pCoreEnumValue> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<NEO::criProductEnumValue, SamplerProductFamily<NEO::criProductEnumValue>> populateCRI;

} // namespace L0
