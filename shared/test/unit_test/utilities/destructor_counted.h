/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/test.h"

namespace NEO {

template <typename BaseType, uint32_t ordinal>
struct DestructorCounted : public BaseType {
    template <typename... Args>
    DestructorCounted(uint32_t &destructorId, Args &&...args) : BaseType(std::forward<Args>(args)...),
                                                                destructorId(destructorId) {}

    ~DestructorCounted() override {
        EXPECT_EQ(ordinal, destructorId);
        destructorId++;
    }

  private:
    uint32_t &destructorId;
};
} // namespace NEO
