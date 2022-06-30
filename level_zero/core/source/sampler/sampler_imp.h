/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/sampler/sampler.h"

namespace L0 {

class SamplerImp : public Sampler {
  public:
    ze_result_t destroy() override;
    virtual ze_result_t initialize(Device *device, const ze_sampler_desc_t *desc);
};

} // namespace L0
