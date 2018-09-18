/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_memory_base.h"

namespace OCLRT {
class GmmMemory : public GmmMemoryBase {
  public:
    static GmmMemory *create();
    virtual ~GmmMemory() = default;

  protected:
    GmmMemory() = default;
};
} // namespace OCLRT
