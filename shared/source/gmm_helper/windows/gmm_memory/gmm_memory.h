/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/windows/gmm_memory_base.h"

namespace NEO {
class GmmMemory : public GmmMemoryBase {
  public:
    static GmmMemory *create(GmmClientContext *gmmClientContext);
    ~GmmMemory() override = default;

  protected:
    using GmmMemoryBase::GmmMemoryBase;
};
} // namespace NEO
