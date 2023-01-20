/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

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
