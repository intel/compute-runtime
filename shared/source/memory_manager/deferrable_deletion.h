/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
class DeferrableDeletion {
  public:
    virtual ~DeferrableDeletion() = default;
    template <typename... Args>
    static DeferrableDeletion *create(Args... args);
    virtual bool apply() = 0;

    bool isExternalHostptr() const { return externalHostptr; }
    bool externalHostptr = false;
};
} // namespace NEO
