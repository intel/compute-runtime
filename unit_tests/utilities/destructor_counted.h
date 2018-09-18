/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "test.h"

namespace OCLRT {

template <typename BaseType, uint32_t ordinal>
struct DestructorCounted : public BaseType {
    template <typename... Args>
    DestructorCounted(uint32_t &destructorId, Args... args) : BaseType(args...),
                                                              destructorId(destructorId) {}

    ~DestructorCounted() override {
        EXPECT_EQ(ordinal, destructorId);
        destructorId++;
    }

  private:
    uint32_t &destructorId;
};
} // namespace OCLRT
