/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_memory_base.h"

namespace NEO {
class GmmMemory : public GmmMemoryBase {
  public:
    static GmmMemory *create(GmmClientContext *gmmClientContext);
    virtual ~GmmMemory() = default;

  protected:
    using GmmMemoryBase::GmmMemoryBase;
};
} // namespace NEO
