/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/idlist.h"

namespace OCLRT {
class DeferrableDeletion : public IDNode<DeferrableDeletion> {
  public:
    template <typename... Args>
    static DeferrableDeletion *create(Args... args);
    virtual void apply() = 0;
    virtual ~DeferrableDeletion() = default;
};
} // namespace OCLRT
