/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

template <typename Args>
struct CapturedApi {
    size_t count() const { return records.size(); }
    bool wasCalled() const { return records.empty() == false; }

    const Args &operator[](size_t index) const { return records.at(index); }

    std::vector<Args> records{};
};

} // namespace ult
} // namespace LEO
} // namespace NEO
