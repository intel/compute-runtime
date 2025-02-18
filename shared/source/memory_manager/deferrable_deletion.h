/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/idlist.h"

namespace NEO {
class DeferrableDeletion : public IDNode<DeferrableDeletion> {
  public:
    template <typename... Args>
    static DeferrableDeletion *create(Args... args);
    virtual bool apply() = 0;

    bool isExternalHostptr() const { return externalHostptr; }
    bool externalHostptr = false;
};

static_assert(NEO::NonCopyableAndNonMovable<IDList<DeferrableDeletion>>);
} // namespace NEO
