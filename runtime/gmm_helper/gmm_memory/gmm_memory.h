/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_memory_base.h"

namespace NEO {
class GmmMemory : public GmmMemoryBase {
  public:
    static GmmMemory *create();
    virtual ~GmmMemory() = default;

  protected:
    GmmMemory() = default;
};
} // namespace NEO
