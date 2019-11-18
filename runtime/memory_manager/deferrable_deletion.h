/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/utilities/idlist.h"

namespace NEO {
class DeferrableDeletion : public IDNode<DeferrableDeletion> {
  public:
    template <typename... Args>
    static DeferrableDeletion *create(Args... args);
    virtual bool apply() = 0;
};
} // namespace NEO
